#include <stdio.h>
#include <stdlib.h>

#include "def.h"
#include "syntaxtree.h"
#include "idtable.h"

extern int yydebug;

bool gError = false;

idTable* gIdTable = NULL;
SyntaxTreeNode* gTree = NULL;

void yyrestart(FILE*);
int yyparse(void);

int main(int argc, char* argv[]) {
	if (argc <= 1) return 1;
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		perror(argv[1]);
		return 1;	
	}
	gIdTable = createIdTable(64);
	//yydebug = 1;
	yyrestart(f);
	yyparse();
	if (!gError) {
		// TODO: semantic analysis
	}
	freeTree(gTree);
	freeTable(gIdTable);
	return 0;
}
