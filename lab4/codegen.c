#include <stdlib.h>

#include "codegen.h"

static int argCount = 0;

void createHeader(FILE* out) {
	char *header = ".data\n"
		"  _prompt: .asciiz \"please enter an integer: \"\n"
		"  _ret: .asciiz \"\\n\"\n"
		"  .globl main\n"
		".text\n";
	fprintf(out, "%s", header);
}

void createReadFunc(FILE* out) {
	fprintf(out, "read:\n"
			"  li $v0, 4\n"
			"  la $a0, _prompt\n"
			"  syscall\n"
			"  li $v0, 5\n"
			"  syscall\n"
			"  jr $ra\n");
}

void createWriteFunc(FILE* out) {
	fprintf(out, "write:\n"
			"  li $v0, 1\n"
			"  syscall\n"
			"  li $v0, 4\n"
			"  la $a0, _ret\n"
			"  syscall\n"
			"  move $v0, $0\n"
			"  jr $ra\n");
}

void getOpName(IROperand* op, char* buffer);

void loadOperand(IROperand* op, int reg, TrieNode* varStackPos, FILE* out) {
	if (op->type == IRO_CONST) {
		fprintf(out, "  li $t%d, %d\n", reg, op->constValue);
	}
	else if (op->type == IRO_TEMP || op->type == IRO_VAR) {
		char varName[20];
		getOpName(op, varName);
		fprintf(out, "  lw $t%d, -%d($fp)\n", reg, queryNode(varStackPos, varName)->intValue + 4);
	}
	else if (op->type == IRO_GETADDR) {
		char varName[20];
		getOpName(op, varName);
		fprintf(out, "  la $t%d, -%d($fp)\n", reg, queryNode(varStackPos, varName)->intValue + 4);
	}
	else if (op->type == IRO_SETADDR) {
		char varName[20];
		getOpName(op, varName);
		fprintf(out, "  lw $t%d, -%d($fp)\n", reg, queryNode(varStackPos, varName)->intValue + 4);
		fprintf(out, "  lw $t%d, 0($t%d)\n", reg, reg);
	}
}

void saveOperand(IROperand* op, int destReg, int backupReg, TrieNode* varStackPos, FILE* out) {
	if (op->type == IRO_TEMP || op->type == IRO_VAR) {
		char varName[20];
		getOpName(op, varName);
		fprintf(out, "  sw $t%d, -%d($fp)\n", destReg, queryNode(varStackPos, varName)->intValue + 4);
	}
	else if (op->type == IRO_SETADDR) {
		char varName[20];
		getOpName(op, varName);
		fprintf(out, "  lw $t%d, -%d($fp)\n", backupReg, queryNode(varStackPos, varName)->intValue + 4);
		fprintf(out, "  sw $t%d, 0($t%d)\n", destReg, backupReg);
	}
}

int computeParamLeft(IRCode* code) {
	int count = 1;
	IRCode* curr = code->next;
	while (curr->type == IR_PARAM) {
		++ count;
		curr = curr->next;
	}
	return count;
}

void _translateIRCode(IRCode* code, FILE* out, TrieNode* funcStackSize, TrieNode* varStackPos) {
	int paramOffset = 0;
	char* rel = NULL;
	switch (code->type) {
	case IR_LABEL:
		printOperand(code->singleOp.op, out);
		fprintf(out, ":\n");
		break;
	case IR_FUNC:
		printOperand(code->singleOp.op, out);
		fprintf(out, ":\n"
				"  addi $sp, $sp, -4\n"
				"  sw $fp, 0($sp)\n"
				"  move $fp, $sp\n");
		fprintf(out, "  addi $sp, $sp, -%d\n", queryNode(funcStackSize, code->singleOp.op->funcName)->intValue);
		break;
	case IR_ASSIGN:
		loadOperand(code->singleOp.op, 0, varStackPos, out);
		saveOperand(code->singleOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_ADD:
		loadOperand(code->binOp.left, 0, varStackPos, out);
		loadOperand(code->binOp.right, 1, varStackPos, out);
		fprintf(out, "  add $t0, $t0, $t1\n");
		saveOperand(code->binOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_MINUS:
		loadOperand(code->binOp.left, 0, varStackPos, out);
		loadOperand(code->binOp.right, 1, varStackPos, out);
		fprintf(out, "  sub $t0, $t0, $t1\n");
		saveOperand(code->binOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_STAR:
		loadOperand(code->binOp.left, 0, varStackPos, out);
		loadOperand(code->binOp.right, 1, varStackPos, out);
		fprintf(out, "  mul $t0, $t0, $t1\n");
		saveOperand(code->binOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_DIV:
		loadOperand(code->binOp.left, 0, varStackPos, out);
		loadOperand(code->binOp.right, 1, varStackPos, out);
		fprintf(out, "  div $t0, $t1\n");
		fprintf(out, "  mflo $t0\n");
		saveOperand(code->binOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_GOTO:
		fprintf(out, "  j ");
		printOperand(code->singleOp.op, out);
		fprintf(out, "\n");
		break;
	case IR_COND:
		loadOperand(code->condOp.left, 0, varStackPos, out);
		loadOperand(code->condOp.right, 1, varStackPos, out);
		switch (code->condOp.relOp) {
		case IR_LT: rel = "blt"; break;
		case IR_LE: rel = "ble"; break;
		case IR_EQ: rel = "beq"; break;
		case IR_GE: rel = "bge"; break;
		case IR_GT: rel = "bgt"; break;
		case IR_NE: rel = "bne"; break;
		}
		fprintf(out, "  %s $t0, $t1, ", rel);
		printOperand(code->condOp.dest, out);
		fprintf(out, "\n");
		break;
	case IR_RETURN:
		loadOperand(code->singleOp.op, 0, varStackPos, out);
		fprintf(out, "  move $v0, $t0\n" // return value
				"  move $sp, $fp\n" // mov esp, ebp
				"  lw $fp, 0($sp)\n"
				"  addi $sp, $sp, 4\n" // pop ebp
				"  jr $ra\n");
		break;
	case IR_DEC:
		break;
	case IR_ARG:
		loadOperand(code->singleOp.op, 0, varStackPos, out);
		fprintf(out, "  addi $sp, $sp, -4\n"
				"  sw $t0, 0($sp)\n");
		++ argCount;
		break;
	case IR_CALL:
		fprintf(out, "  addi $sp, $sp, -4\n"
				"  sw $ra, 0($sp)\n"); // push $ra
		fprintf(out, "  jal ");
		printOperand(code->singleOp.op, out);
		fprintf(out, "\n"
				"  move $t0, $v0\n");
		saveOperand(code->singleOp.dest, 0, 1, varStackPos, out);
		fprintf(out, "  lw $ra, 0($sp)\n" // pop $ra and arguments
				"  addi $sp, $sp, %d\n", 4 * (1 + argCount));
		argCount = 0;
		break;
	case IR_PARAM:
		paramOffset = computeParamLeft(code) + 1;
		fprintf(out, "  lw $t0, %d($fp)\n", paramOffset * 4); // load argument to stack
		saveOperand(code->singleOp.op, 0, 1, varStackPos, out);
		break;
	case IR_READ:
		fprintf(out, "  addi $sp, $sp, -4\n"
				"  sw $ra, 0($sp)\n"
				"  jal read\n"
				"  lw $ra, 0($sp)\n"
				"  addi $sp, $sp, 4\n"
				"  move $t0, $v0\n");
		saveOperand(code->singleOp.dest, 0, 1, varStackPos, out);
		break;
	case IR_WRITE:
		loadOperand(code->singleOp.op, 0, varStackPos, out);
		fprintf(out, "  move $a0, $t0\n"
				"  addi $sp, $sp, -4\n"
				"  sw $ra, 0($sp)\n"
				"  jal write\n"
				"  lw $ra, 0($sp)\n"
				"  addi $sp, $sp, 4\n");
		break;
	}
}

void getOpName(IROperand* op, char* buffer) {
	if (op->type == IRO_TEMP
			|| (op->type != IRO_VAR && op->isTemp)) {
		sprintf(buffer, "t%d", op->tempId);
	}
	else {
		sprintf(buffer, "v%d", op->varId);
	}
}

int insertVar(TrieNode* stackPos, IROperand* op, int offset, int size) {
	if (op->type == IRO_CONST
			|| op->type == IRO_FUNC
			|| op->type == IRO_LABEL) {
		return 0;
	}
	char funcName[20];
	getOpName(op, funcName);
	if (queryNode(stackPos, funcName) == NULL) {
		TrieNode* newNode = insertTrie(stackPos, funcName, NULL);
		newNode->intValue = offset;
		return size;
	}
	else {
		return 0;
	}
}

void printNode(TrieNode* node) {
	if (!node->isEnd) {
		return;
	}
	char* name = retrieveStr(node);
	printf("%s: %d\n", name, node->intValue);
	free(name);
}

void freeNode(TrieNode* node) {
	free(node);
}

void analyzeStackInfo(IRCode* code, TrieNode** funcStackSize, TrieNode** varStackPos) {
	TrieNode* stackSize = initTrieNode(NULL, -1);
	TrieNode* stackPos = initTrieNode(NULL, -1);

	char* currFuncName = NULL;
	TrieNode* funcNode = NULL;
	IRCode* head = code;
	IRCode* curr = code;
	do {
		int size = 4;
		switch (curr->type) {
		case IR_LABEL:
			break;
		case IR_FUNC:
			currFuncName = curr->singleOp.op->funcName;
			funcNode = insertTrie(stackSize, currFuncName, NULL);
			funcNode->intValue = 0;
			break;
		case IR_ASSIGN:
			funcNode->intValue += insertVar(stackPos, curr->singleOp.op, funcNode->intValue, size);
			funcNode->intValue += insertVar(stackPos, curr->singleOp.dest, funcNode->intValue, size);
			break;
		case IR_ADD:
		case IR_MINUS:
		case IR_STAR:
		case IR_DIV:
			funcNode->intValue += insertVar(stackPos, curr->binOp.left, funcNode->intValue, size);
			funcNode->intValue += insertVar(stackPos, curr->binOp.right, funcNode->intValue, size);
			funcNode->intValue += insertVar(stackPos, curr->binOp.dest, funcNode->intValue, size);
			break;
		case IR_GOTO:
			break;
		case IR_COND:
			funcNode->intValue += insertVar(stackPos, curr->condOp.left, funcNode->intValue, size);
			funcNode->intValue += insertVar(stackPos, curr->condOp.right, funcNode->intValue, size);
			funcNode->intValue += insertVar(stackPos, curr->condOp.dest, funcNode->intValue, size);
			break;
		case IR_RETURN:
		case IR_ARG:
		case IR_PARAM:
		case IR_WRITE:
			funcNode->intValue += insertVar(stackPos, curr->singleOp.op, funcNode->intValue, size);
			break;
		case IR_DEC:
			size = curr->decOp.size;
			funcNode->intValue += insertVar(stackPos, curr->decOp.op, funcNode->intValue, size);
			break;
		case IR_CALL:
		case IR_READ:
			funcNode->intValue += insertVar(stackPos, curr->singleOp.dest, funcNode->intValue, size);
			break;
		}
		curr = curr->next;
	} while (curr != head);

	*funcStackSize = stackSize;
	*varStackPos = stackPos;
}

void translateIRCode(IRCode* code, FILE* out) {
	if (code == NULL) return;
	TrieNode* funcStackSize = NULL;
	TrieNode* varStackPos = NULL;
	analyzeStackInfo(code, &funcStackSize, &varStackPos);
	//traverseTrie(funcStackSize, printNode, NULL);
	//traverseTrie(varStackPos, printNode, NULL);
	createHeader(out);
	createReadFunc(out);
	createWriteFunc(out);
	IRCode* head = code;
	IRCode* curr = head;
	IRCode* next = curr->next;
	char* currFuncName = NULL;
	_translateIRCode(curr, out, funcStackSize, varStackPos);
	while (next != head) {
		curr = next;
		next = curr->next;
		_translateIRCode(curr, out, funcStackSize, varStackPos);
	}
	traverseTrie(funcStackSize, NULL, freeNode);
	traverseTrie(varStackPos, NULL, freeNode);
}
