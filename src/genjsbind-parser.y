%{
/* parser for the binding generation config file 
 *
 * This file is part of nsgenjsbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#include <stdio.h>
#include <string.h>

#include "genjsbind-parser.h"
#include "genjsbind-lexer.h"
#include "webidl-ast.h"
#include "genjsbind-ast.h"

char *errtxt;

static void genjsbind_error(const char *str)
{
    errtxt = strdup(str);
}


int genjsbind_wrap()
{
    return 1;
}

%}

%locations
%define api.pure
%error-verbose

%union
{
  char* text;
}

%token TOK_IDLFILE
%token TOK_HDR_COMMENT
%token TOK_PREAMBLE

%token TOK_BINDING
%token TOK_INTERFACE
%token TOK_TYPE
%token TOK_EXTRA
%token TOK_NODE

%token <text> TOK_IDENTIFIER

%token <text> TOK_STRING_LITERAL
%token <text> TOK_CCODE_LITERAL


%%

 /* [1] start with Statements */
Statements
        : 
        Statement 
        | 
        Statements Statement  
        | 
        error ';' 
        { 
            fprintf(stderr, "%d: %s\n", yylloc.first_line, errtxt);
            free(errtxt);
            YYABORT ;
        }
        ;

Statement
        : 
        IdlFile
        | 
        HdrComment
        |
        Preamble
        | 
        Binding
        ;

 /* [3] load a web IDL file */
IdlFile
        : TOK_IDLFILE TOK_STRING_LITERAL ';'
        {
          if (webidl_parsefile($2) != 0) {
            YYABORT;
          }
        }
        ;

HdrComment
        : TOK_HDR_COMMENT HdrStrings ';'
        {
          
        }
        ;

HdrStrings
        : 
        TOK_STRING_LITERAL
        {
          genbind_header_comment($1);
        }
        | 
        HdrStrings TOK_STRING_LITERAL 
        {
          genbind_header_comment($2);
        }
        ;

Preamble
        :
        TOK_PREAMBLE CBlock ';'
        ;

CBlock
        : 
        TOK_CCODE_LITERAL
        {
          genbind_preamble($1);
        }
        | 
        CBlock TOK_CCODE_LITERAL 
        {
          genbind_preamble($2);
        }
        ;

Binding
        :
        TOK_BINDING TOK_IDENTIFIER '{' BindingArgs '}' ';'
        ;

BindingArgs
        :
        BindingArg
        |
        BindingArgs BindingArg
        ;

BindingArg
        : 
        Type
        | 
        Extra
        | 
        Interface
        ;

Type
        :
        TOK_TYPE TOK_IDENTIFIER '{' TypeArgs '}' ';'
        ;

TypeArgs
        :
        TOK_NODE TOK_IDENTIFIER ';'
        ;

Extra
        :
        TOK_EXTRA TOK_STRING_LITERAL ';'
        ;

Interface
        : 
        TOK_INTERFACE TOK_IDENTIFIER ';'
        {
          genbind_interface($2);
        }
        ;

%%
