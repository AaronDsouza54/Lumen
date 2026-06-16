#include "compiler.h"

#include <string.h>

#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Parser parser;
Compiler *current = null;
Chunk *compilingChunk;

static void call(bool canAssign);
static void unary(bool canAssign);
static void string(bool canAssign);
static void number(bool canAssign);
static void binary(bool canAssign);
static void literal(bool canAssign);
static void variable(bool canAssign);
static void grouping(bool canAssign);

static void expression();
static void statement();
static void declaration();
static void varDeclaration();
static void funcDeclaration();
static void or_(bool canAssign);
static void function(FunctionType type);
static void and_(bool canAssign);
static void patchJump(int32_t offset);
static uint8_t makeConstant(Value value);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void namedVariable(Token name, bool canAssign);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {null, null, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {null, null, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {null, null, PREC_NONE},
    [TOKEN_COMMA] = {null, null, PREC_NONE},
    [TOKEN_DOT] = {null, null, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {null, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {null, null, PREC_NONE},
    [TOKEN_SLASH] = {null, binary, PREC_FACTOR},
    [TOKEN_STAR] = {null, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, null, PREC_UNARY},
    [TOKEN_BANG_EQUAL] = {null, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {null, binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL] = {null, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {null, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {null, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {null, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {null, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, null, PREC_NONE},
    [TOKEN_STRING] = {string, null, PREC_NONE},
    [TOKEN_NUMBER] = {number, null, PREC_NONE},
    [TOKEN_AND] = {null, and_, PREC_AND},
    [TOKEN_CLASS] = {null, null, PREC_NONE},
    [TOKEN_ELSE] = {null, null, PREC_NONE},
    [TOKEN_FALSE] = {literal, null, PREC_NONE},
    [TOKEN_FOR] = {null, null, PREC_NONE},
    [TOKEN_FUNC] = {null, null, PREC_NONE},
    [TOKEN_IF] = {null, null, PREC_NONE},
    [TOKEN_NULL] = {literal, null, PREC_NONE},
    [TOKEN_OR] = {null, or_, PREC_OR},
    [TOKEN_PRINT] = {null, null, PREC_NONE},
    [TOKEN_RETURN] = {null, null, PREC_NONE},
    [TOKEN_SUPER] = {null, null, PREC_NONE},
    [TOKEN_THIS] = {null, null, PREC_NONE},
    [TOKEN_TRUE] = {literal, null, PREC_NONE},
    [TOKEN_VAR] = {null, null, PREC_NONE},
    [TOKEN_WHILE] = {null, null, PREC_NONE},
    [TOKEN_ERROR] = {null, null, PREC_NONE},
    [TOKEN_EOF] = {null, null, PREC_NONE},
};

static Chunk *currentChunk() {
  return &current->function->chunk;
}

static void errorAt(const Token *token, const char *message) {
  if (parser.panicMode)
    return;

  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
  printf("Error called from consume function\n");
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitReturn() {
  emitByte(OP_NULL);
  emitByte(OP_RETURN);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static int32_t emitJump(const uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitLoop(const int32_t loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static ObjFunction* endCompiler() {
  emitReturn();
  ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != null
                                      ? function->name->chars : "<script>");
  }
#endif

  current = current->enclosing;

  return function;
}

static ParseRule *getRule(const TokenType type) { return &rules[type]; }

static void parsePrecedence(const Precedence precedence) {
  advance();
  // printf("%d", parser.previous.type);
  const ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == null) {
    error("Expect expression.");
    return;
  }

  const bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    const ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void addLocal(const Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in the function");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;

  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static uint8_t identifierConstant(const Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0)
    return 0;

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0) return; 
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitBytes(OP_DEFINE_GLOBAL, global);
}

static void and_(bool canAssign) {
  const int32_t endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static void or_(bool canAssign) {
  const int32_t elseJump = emitJump(OP_JUMP_IF_FALSE);
  const int32_t endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NULL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void funcDeclaration() {
  uint8_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUNC:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:; // Do nothing.
    }

    advance();
  }
}

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(const Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(const int32_t offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  const int32_t jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;

  compiler->localCount = 0;
  compiler->scopeDepth = 0;

  compiler->function = newFunction();

  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start, parser.previous.length);
  }

  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}

static void number(bool canAssign) {
  const char *start = parser.previous.start;
  int32_t length = parser.previous.length;

  bool isFloat = false;

  for (int i = 0; i < length; i++) {
    if (start[i] == '.') {
      isFloat = true;
      break;
    }
  }

  if (isFloat) {
    double value = strtod(start, null);
    emitConstant(NUMBER_VAL(value));
  } else {
    int64_t value = strtoll(start, null, 10);
    emitConstant(INTEGER_VAL(value));
  }
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static int32_t resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }

      return i;
    }
  }

  return -1;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  return -1;
}


static void namedVariable(Token name, const bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(const bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments");
  return argCount;
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static void unary(const bool canAssign) {
  const TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY); // Compile operand.

  // Emit operator instruction.
  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return; // Unreachable.
  }
}

static void binary(bool canAssign) {
  const TokenType operatorType = parser.previous.type;
  const ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_SLASH_SLASH:
    emitByte(OP_INT_DIVIDE);
    break;
  case TOKEN_BANG_EQUAL:
    emitByte(OP_BANG_EQUAL);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitByte(OP_GREATER_EQUAL);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitByte(OP_LESS_EQUAL);
    break;
  default:
    return; // Unreachable
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NULL:
    emitByte(OP_NULL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return; // Unreachable.
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void declaration() {
  if (match(TOKEN_FUNC)) {
    funcDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }
  if (parser.panicMode)
    synchronize();
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void beginScope() { current->scopeDepth++; }

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'function'");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Cannot have more than 255 parameters");
      }
      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'function'");
  consume(TOKEN_LEFT_BRACE, "Expect '{' after 'function'");
  block();

  ObjFunction *function = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
}

static void endScope() {
  current->scopeDepth--;

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount--;
  }
}

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");

  const int32_t thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  const int32_t elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE))
    statement();
  patchJump(elseJump);
}

static void whileStatement() {
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  const int32_t exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
}

static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  // Initializer
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (!match(TOKEN_SEMICOLON)) {
    expressionStatement();
  }

  int32_t loopStart = currentChunk()->count;

  // Condition
  int32_t exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // pop condition
  }

  // Increment
  int32_t bodyJump = -1;
  if (!match(TOKEN_RIGHT_PAREN)) {
    bodyJump = emitJump(OP_JUMP); // jump over increment
    int32_t incrementStart = currentChunk()->count;
    expression(); // increment
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after clauses.");

    emitLoop(loopStart); // loop back to condition
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  // Body
  statement();

  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // pop condition
  }

  endScope();
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  }  else {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

ObjFunction* compile(const char* source) {
  initScanner(source);

  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction* function = endCompiler();
  return  parser.hadError ? NULL : function;
}
