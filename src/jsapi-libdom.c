/* binding output generator for jsapi(spidermonkey) to libdom
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "options.h"
#include "nsgenbind-ast.h"
#include "webidl-ast.h"
#include "jsapi-libdom.h"

#define HDR_COMMENT_SEP     "\n * "
#define HDR_COMMENT_PREABLE "Generated by nsgenbind "


static int webidl_file_cb(struct genbind_node *node, void *ctx)
{
	struct webidl_node **webidl_ast = ctx;
	char *filename;

	filename = genbind_node_gettext(node);

	return webidl_parsefile(filename, webidl_ast);

}

static int
read_webidl(struct genbind_node *genbind_ast, struct webidl_node **webidl_ast)
{
	int res;

	res = genbind_node_for_each_type(genbind_ast,
					 GENBIND_NODE_TYPE_WEBIDLFILE,
					 webidl_file_cb,
					 webidl_ast);

	/* debug dump of web idl AST */
	if (options->verbose) {
		webidl_ast_dump(*webidl_ast, 0);
	}
	return res;
}


static int webidl_preamble_cb(struct genbind_node *node, void *ctx)
{
	struct binding *binding = ctx;

	fprintf(binding->outfile, "%s", genbind_node_gettext(node));

	return 0;
}


static int webidl_hdrcomments_cb(struct genbind_node *node, void *ctx)
{
	struct binding *binding = ctx;

	fprintf(binding->outfile,
		HDR_COMMENT_SEP"%s",
		genbind_node_gettext(node));

	return 0;
}

static int webidl_hdrcomment_cb(struct genbind_node *node, void *ctx)
{
	genbind_node_for_each_type(genbind_node_getnode(node),
				   GENBIND_NODE_TYPE_STRING,
				   webidl_hdrcomments_cb,
				   ctx);
	return 0;
}

static int webidl_property_spec_cb(struct webidl_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct webidl_node *ident_node;
	struct webidl_node *modifier_node;

	ident_node = webidl_node_find(webidl_node_getnode(node),
				      NULL,
				      webidl_cmp_node_type,
				      (void *)WEBIDL_NODE_TYPE_IDENT);

	modifier_node = webidl_node_find(webidl_node_getnode(node),
					 NULL,
					 webidl_cmp_node_type,
					 (void *)WEBIDL_NODE_TYPE_MODIFIER);

	if (ident_node == NULL) {
		/* properties must have an operator
		 * http://www.w3.org/TR/WebIDL/#idl-attributes
		 */
		return 1;
	} else {
		if (webidl_node_getint(modifier_node) == WEBIDL_TYPE_READONLY) {
			fprintf(binding->outfile,
				"    JSAPI_PS_RO(%s, 0, JSPROP_ENUMERATE | JSPROP_SHARED),\n",
				webidl_node_gettext(ident_node));
		} else {
			fprintf(binding->outfile,
				"    JSAPI_PS(%s, 0, JSPROP_ENUMERATE | JSPROP_SHARED),\n",
				webidl_node_gettext(ident_node));
		}
	}
	return 0;
}

static int
generate_property_spec(struct binding *binding, const char *interface)
{
	struct webidl_node *interface_node;
	struct webidl_node *members_node;
	struct webidl_node *inherit_node;


	/* find interface in webidl with correct ident attached */
	interface_node = webidl_node_find_type_ident(binding->wi_ast,
						     WEBIDL_NODE_TYPE_INTERFACE,
						     interface);

	if (interface_node == NULL) {
		fprintf(stderr,
			"Unable to find interface %s in loaded WebIDL\n",
			interface);
		return -1;
	}

	members_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_LIST);

	while (members_node != NULL) {

		fprintf(binding->outfile,"    /**** %s ****/\n", interface);


		/* for each function emit a JSAPI_FS()*/
		webidl_node_for_each_type(webidl_node_getnode(members_node),
					  WEBIDL_NODE_TYPE_ATTRIBUTE,
					  webidl_property_spec_cb,
					  binding);


		members_node = webidl_node_find(webidl_node_getnode(interface_node),
						members_node,
						webidl_cmp_node_type,
						(void *)WEBIDL_NODE_TYPE_LIST);

	}
	/* check for inherited nodes and insert them too */
	inherit_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE);

	if (inherit_node != NULL) {
		return generate_property_spec(binding,
					      webidl_node_gettext(inherit_node));
	}

	return 0;
}



static int webidl_func_spec_cb(struct webidl_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct webidl_node *ident_node;

	ident_node = webidl_node_find(webidl_node_getnode(node),
				      NULL,
				      webidl_cmp_node_type,
				      (void *)WEBIDL_NODE_TYPE_IDENT);

	if (ident_node == NULL) {
		/* operation without identifier - must have special keyword
		 * http://www.w3.org/TR/WebIDL/#idl-operations
		 */
	} else {
		fprintf(binding->outfile,
			"    JSAPI_FS(%s, 0, 0),\n",
			webidl_node_gettext(ident_node));
	}
	return 0;
}

static int
generate_function_spec(struct binding *binding, const char *interface)
{
	struct webidl_node *interface_node;
	struct webidl_node *members_node;
	struct webidl_node *inherit_node;

	/* find interface in webidl with correct ident attached */
	interface_node = webidl_node_find_type_ident(binding->wi_ast,
						     WEBIDL_NODE_TYPE_INTERFACE,
						     interface);

	if (interface_node == NULL) {
		fprintf(stderr,
			"Unable to find interface %s in loaded WebIDL\n",
			interface);
		return -1;
	}

	members_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_LIST);
	while (members_node != NULL) {

		fprintf(binding->outfile,"    /**** %s ****/\n", interface);

		/* for each function emit a JSAPI_FS()*/
		webidl_node_for_each_type(webidl_node_getnode(members_node),
					  WEBIDL_NODE_TYPE_OPERATION,
					  webidl_func_spec_cb,
					  binding);

		members_node = webidl_node_find(webidl_node_getnode(interface_node),
						members_node,
						webidl_cmp_node_type,
						(void *)WEBIDL_NODE_TYPE_LIST);
	}

	/* check for inherited nodes and insert them too */
	inherit_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE);

	if (inherit_node != NULL) {
		return generate_function_spec(binding,
					      webidl_node_gettext(inherit_node));
	}

	return 0;
}








static int webidl_property_body_cb(struct webidl_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct webidl_node *ident_node;
	struct webidl_node *modifier_node;

	ident_node = webidl_node_find(webidl_node_getnode(node),
				      NULL,
				      webidl_cmp_node_type,
				      (void *)WEBIDL_NODE_TYPE_IDENT);

	if (ident_node == NULL) {
		/* properties must have an operator
		 * http://www.w3.org/TR/WebIDL/#idl-attributes
		 */
		return 1;
	}

	modifier_node = webidl_node_find(webidl_node_getnode(node),
					 NULL,
					 webidl_cmp_node_type,
					 (void *)WEBIDL_NODE_TYPE_MODIFIER);


	if (webidl_node_getint(modifier_node) != WEBIDL_TYPE_READONLY) {
		/* no readonly so a set function is required */

		fprintf(binding->outfile,
			"static JSBool JSAPI_PROPERTYSET(%s, JSContext *cx, JSObject *obj, jsval *vp)\n",
			webidl_node_gettext(ident_node));
		fprintf(binding->outfile,
			"{\n"
			"        return JS_FALSE;\n"
			"}\n\n");
	}

	fprintf(binding->outfile,
		"static JSBool JSAPI_PROPERTYGET(%s, JSContext *cx, JSObject *obj, jsval *vp)\n",
		webidl_node_gettext(ident_node));
	fprintf(binding->outfile,
		"{\n"
		"	JS_SET_RVAL(cx, vp, JSVAL_NULL);\n"
		"	return JS_TRUE;\n");
	fprintf(binding->outfile, "}\n\n");


	return 0;
}



static int webidl_private_cb(struct genbind_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct genbind_node *ident_node;
	struct genbind_node *type_node;


	ident_node = genbind_node_find_type(genbind_node_getnode(node),
					    NULL,
					    GENBIND_NODE_TYPE_IDENT);
	if (ident_node == NULL)
		return -1; /* bad AST */

	type_node = genbind_node_find_type(genbind_node_getnode(node),
					    NULL,
					    GENBIND_NODE_TYPE_STRING);
	if (type_node == NULL)
		return -1; /* bad AST */

	fprintf(binding->outfile,
		"        %s%s;\n",
		genbind_node_gettext(type_node),
		genbind_node_gettext(ident_node));

	return 0;
}

static int webidl_private_param_cb(struct genbind_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct genbind_node *ident_node;
	struct genbind_node *type_node;


	ident_node = genbind_node_find_type(genbind_node_getnode(node),
					    NULL,
					    GENBIND_NODE_TYPE_IDENT);
	if (ident_node == NULL)
		return -1; /* bad AST */

	type_node = genbind_node_find_type(genbind_node_getnode(node),
					    NULL,
					    GENBIND_NODE_TYPE_STRING);
	if (type_node == NULL)
		return -1; /* bad AST */

	fprintf(binding->outfile,
		",\n\t\t%s%s",
		genbind_node_gettext(type_node),
		genbind_node_gettext(ident_node));

	return 0;
}

static int webidl_private_assign_cb(struct genbind_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct genbind_node *ident_node;
	struct genbind_node *type_node;

	ident_node = genbind_node_find_type(genbind_node_getnode(node),
					    NULL,
					    GENBIND_NODE_TYPE_IDENT);
	if (ident_node == NULL)
		return -1; /* bad AST */

	fprintf(binding->outfile,
		"\tprivate->%1$s = %1$s;\n",
		genbind_node_gettext(ident_node));

	return 0;
}


static int
output_con_de_structors(struct binding *binding)
{
	int res = 0;
	struct genbind_node *binding_node;

	binding_node = genbind_node_find(binding->gb_ast,
					 NULL,
					 genbind_cmp_node_type,
					 (void *)GENBIND_NODE_TYPE_BINDING);

	if (binding_node == NULL) {
		return -1;
	}

	/* finalize */
	fprintf(binding->outfile,
		"static void jsclass_finalize(JSContext *cx, JSObject *obj)\n"
		"{"
		"\tstruct jsclass_private *private;\n"
		"\n"
		"\tprivate = JS_GetInstancePrivate(cx, obj, &JSClass_%s, NULL);\n"
		"\tif (private != NULL) {\n"
		"\t\tfree(private);\n"
		"\t}\n"
		"}\n\n", 
		binding->interface);

	/* resolve */
	fprintf(binding->outfile,
		"static JSBool jsclass_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)\n"
		"{\n"
		"\t*objp = NULL;\n"
		"\treturn JS_TRUE;\n"
		"}\n\n");

	/* class Initialisor */
	fprintf(binding->outfile,
		"JSObject *jsapi_InitClass_%1$s(JSContext *cx, JSObject *parent)\n"
		"{\n"
		"\tJSObject *jsobject;\n"
		"\n"
		"\tjsobject = JS_InitClass(cx,\n"
		"\t\tparent,\n"
		"\t\tNULL,\n"
		"\t\t&JSClass_%1$s,\n"
		"\t\tNULL,\n"
		"\t\t0,\n"
		"\t\tjsclass_properties,\n"
		"\t\tjsclass_functions, \n"
		"\t\tNULL, \n"
		"\t\tNULL);\n"
		"\treturn jsobject;\n"
		"}\n\n",
		binding->interface);


	/* constructor */
	fprintf(binding->outfile,
		"JSObject *jsapi_new_%s(JSContext *cx,\n"
		"\t\tJSObject *proto,\n"
		"\t\tJSObject *parent",
		binding->interface);

	genbind_node_for_each_type(genbind_node_getnode(binding_node),
				   GENBIND_NODE_TYPE_BINDING_PRIVATE,
				   webidl_private_param_cb,
				   binding);

	fprintf(binding->outfile,
		")\n"
		"{\n"
		"\tJSObject *jsobject;\n"
		"\tstruct jsclass_private *private;\n"
		"\n"
		"\tprivate = malloc(sizeof(struct jsclass_private));\n"
		"\tif (private == NULL) {\n"
		"\t\treturn NULL;\n"
		"\t}\n");

	genbind_node_for_each_type(genbind_node_getnode(binding_node),
				   GENBIND_NODE_TYPE_BINDING_PRIVATE,
				   webidl_private_assign_cb,
				   binding);

	fprintf(binding->outfile,
		"\n"
		"\tjsobject = JS_NewObject(cx, &JSClass_%s, proto, parent);\n"
		"\tif (jsobject == NULL) {\n"
		"\t\tfree(private);\n"
		"\t\treturn NULL;\n"
		"\t}\n"
		"\n"
		"\t/* attach private pointer */\n"
		"\tif (JS_SetPrivate(cx, jsobject, private) != JS_TRUE) {\n"
		"\t\tfree(private);\n"
		"\t\treturn NULL;\n"
		"\t}\n"
		"\n"
		"\treturn jsobject;\n"
		"}\n",
		binding->interface);


	return res;
}

static int
output_property_spec(struct binding *binding)
{
	int res;

	fprintf(binding->outfile,
		"static JSPropertySpec jsclass_properties[] = {\n");

	res = generate_property_spec(binding, binding->interface);

	fprintf(binding->outfile, "    JSAPI_PS_END\n};\n\n");

	return res;
}

static int
output_function_spec(struct binding *binding)
{
	int res;

	fprintf(binding->outfile,
		"static JSFunctionSpec jsclass_functions[] = {\n");

	res = generate_function_spec(binding, binding->interface);

	fprintf(binding->outfile, "   JSAPI_FS_END\n};\n\n");

	return res;
}

static int
output_property_body(struct binding *binding, const char *interface)
{
	struct webidl_node *interface_node;
	struct webidl_node *members_node;
	struct webidl_node *inherit_node;


	/* find interface in webidl with correct ident attached */
	interface_node = webidl_node_find_type_ident(binding->wi_ast,
						     WEBIDL_NODE_TYPE_INTERFACE,
						     interface);

	if (interface_node == NULL) {
		fprintf(stderr,
			"Unable to find interface %s in loaded WebIDL\n",
			interface);
		return -1;
	}

	members_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_LIST);

	while (members_node != NULL) {

		fprintf(binding->outfile,"/**** %s ****/\n", interface);

		/* for each function emit property body */
		webidl_node_for_each_type(webidl_node_getnode(members_node),
					  WEBIDL_NODE_TYPE_ATTRIBUTE,
					  webidl_property_body_cb,
					  binding);


		members_node = webidl_node_find(webidl_node_getnode(interface_node),
						members_node,
						webidl_cmp_node_type,
						(void *)WEBIDL_NODE_TYPE_LIST);

	}
	/* check for inherited nodes and insert them too */
	inherit_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE);

	if (inherit_node != NULL) {
		return output_property_body(binding,
					    webidl_node_gettext(inherit_node));
	}

	return 0;
}


static int
output_jsclass(struct binding *binding)
{
	/* forward declare the resolver and finalizer */
	fprintf(binding->outfile,
		"static void jsclass_finalize(JSContext *cx, JSObject *obj);\n");
	fprintf(binding->outfile,
		"static JSBool jsclass_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp);\n\n");

	/* output the class */
	fprintf(binding->outfile,
		"JSClass JSClass_%1$s = {\n"
		"        \"%1$s\",\n"
		"	JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,\n"
		"	JS_PropertyStub,\n"
		"	JS_PropertyStub,\n"
		"	JS_PropertyStub,\n"
		"	JS_StrictPropertyStub,\n"
		"	JS_EnumerateStub,\n"
		"	(JSResolveOp)jsclass_resolve,\n"
		"	JS_ConvertStub,\n"
		"	jsclass_finalize,\n"
		"	JSCLASS_NO_OPTIONAL_MEMBERS\n"
		"};\n\n", 
		binding->interface);
	return 0;
}

static int
output_private_declaration(struct binding *binding)
{
	struct genbind_node *binding_node;
	struct genbind_node *type_node;

	binding_node = genbind_node_find(binding->gb_ast,
					 NULL,
					 genbind_cmp_node_type,
					 (void *)GENBIND_NODE_TYPE_BINDING);

	if (binding_node == NULL) {
		return -1;
	}

	type_node = genbind_node_find(genbind_node_getnode(binding_node),
				      NULL,
				      genbind_cmp_node_type,
				      (void *)GENBIND_NODE_TYPE_BINDING_TYPE);

	if (type_node == NULL) {
		return -1;
	}

	fprintf(binding->outfile, "struct jsclass_private {\n");

	genbind_node_for_each_type(genbind_node_getnode(binding_node),
				   GENBIND_NODE_TYPE_BINDING_PRIVATE,
				   webidl_private_cb,
				   binding);

	fprintf(binding->outfile, "};\n\n");


	return 0;
}

static int
output_preamble(struct binding *binding)
{
	genbind_node_for_each_type(binding->gb_ast,
				   GENBIND_NODE_TYPE_PREAMBLE,
				   webidl_preamble_cb,
				   binding);

	fprintf(binding->outfile,"\n\n");

	return 0;
}

static int
output_header_comments(struct binding *binding)
{
	fprintf(binding->outfile, "/* "HDR_COMMENT_PREABLE);

	genbind_node_for_each_type(binding->gb_ast,
				   GENBIND_NODE_TYPE_HDRCOMMENT,
				   webidl_hdrcomment_cb,
				   binding);

	fprintf(binding->outfile,"\n */\n\n");
	return 0;
}

static struct binding *
binding_new(char *outfilename, struct genbind_node *genbind_ast)
{
	struct binding *nb;
	struct genbind_node *binding_node;
	struct genbind_node *ident_node;
	struct genbind_node *interface_node;
	FILE *outfile ; /* output file */
	struct webidl_node *webidl_ast = NULL;
	int res;

	binding_node = genbind_node_find_type(genbind_ast,
					 NULL,
					 GENBIND_NODE_TYPE_BINDING);
	if (binding_node == NULL) {
		return NULL;
	}

	ident_node = genbind_node_find_type(genbind_node_getnode(binding_node),
					    NULL,
					    GENBIND_NODE_TYPE_IDENT);
	if (ident_node == NULL) {
		return NULL;
	}

	interface_node = genbind_node_find_type(genbind_node_getnode(binding_node),
					   NULL,
					   GENBIND_NODE_TYPE_BINDING_INTERFACE);
	if (interface_node == NULL) {
		return NULL;
	}

	/* walk ast and load any web IDL files required */
	res = read_webidl(genbind_ast, &webidl_ast);
	if (res != 0) {
		fprintf(stderr, "Error reading Web IDL files\n");
		return NULL;
	}

	/* open output file */
	if (outfilename == NULL) {
		outfile = stdout;
	} else {
		outfile = fopen(outfilename, "w");
	}
	if (outfile == NULL) {
		fprintf(stderr, "Error opening output %s: %s\n",
			outfilename,
			strerror(errno));
		return NULL;
	}

	nb = calloc(1, sizeof(struct binding));

	nb->gb_ast = genbind_ast;
	nb->wi_ast = webidl_ast;
	nb->name = genbind_node_gettext(ident_node);
	nb->interface = genbind_node_gettext(interface_node);
	nb->outfile = outfile;

	return nb;
}


int jsapi_libdom_output(char *outfilename, struct genbind_node *genbind_ast)
{
	int res;
	struct binding *binding;

	/* get general binding information used in output */
	binding = binding_new(outfilename, genbind_ast);
	if (binding == NULL) {
		return 4;
	}

	res = output_header_comments(binding);
	if (res) {
		return 5;
	}

	res = output_preamble(binding);
	if (res) {
		return 6;
	}

	res = output_private_declaration(binding);
	if (res) {
		return 7;
	}

	res = output_jsclass(binding);
	if (res) {
		return 8;
	}

	res = output_operator_body(binding, binding->interface);
	if (res) {
		return 9;
	}

	res = output_property_body(binding, binding->interface);
	if (res) {
		return 10;
	}

	res = output_function_spec(binding);
	if (res) {
		return 11;
	}

	res = output_property_spec(binding);
	if (res) {
		return 12;
	}

	res = output_con_de_structors(binding);
	if (res) {
		return 13;
	}

	fclose(binding->outfile);

	return 0;
}
