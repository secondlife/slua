#include "lslmini.hh"
#include "visitor.hh"

namespace Tailslide {


bool ASTVisitor::visitSpecific(LSLASTNode *node) {
  switch(node->getNodeType()) {
    case NODE_NODE:
      return visit(node);
    case NODE_NULL:
      return visit((LSLASTNullNode *)node);
    case NODE_AST_NODE_LIST:
      return visit((LSLASTNodeList<LSLASTNode> *)node);
    case NODE_SCRIPT:
      return visit((LSLScript *)node);
    case NODE_GLOBAL_FUNCTION:
      return visit((LSLGlobalFunction *)node);
    case NODE_GLOBAL_VARIABLE:
      return visit((LSLGlobalVariable *)node);
    case NODE_IDENTIFIER:
      return visit((LSLIdentifier *)node);
    case NODE_CONSTANT: {
      switch(node->getNodeSubType()) {
        case NODE_INTEGER_CONSTANT:
          return visit((LSLIntegerConstant *)node);
        case NODE_FLOAT_CONSTANT:
          return visit((LSLFloatConstant *)node);
        case NODE_STRING_CONSTANT:
          return visit((LSLStringConstant *)node);
        case NODE_KEY_CONSTANT:
          return visit((LSLKeyConstant *)node);
        case NODE_VECTOR_CONSTANT:
          return visit((LSLVectorConstant *)node);
        case NODE_QUATERNION_CONSTANT:
          return visit((LSLQuaternionConstant *)node);
        case NODE_LIST_CONSTANT:
          return visit((LSLListConstant *)node);
        default:
          return visit((LSLConstant *)node);
      }
    }
    case NODE_FUNCTION_DEC:
      return visit((LSLFunctionDec *)node);
    case NODE_EVENT_DEC:
      return visit((LSLEventDec *)node);
    case NODE_STATE:
      return visit((LSLState *)node);
    case NODE_EVENT_HANDLER:
      return visit((LSLEventHandler *)node);
    case NODE_STATEMENT:
      switch(node->getNodeSubType()) {
        case NODE_COMPOUND_STATEMENT:
          return visit((LSLCompoundStatement *)node);
        case NODE_EXPRESSION_STATEMENT:
          return visit((LSLExpressionStatement *)node);
        case NODE_RETURN_STATEMENT:
          return visit((LSLReturnStatement *)node);
        case NODE_LABEL:
          return visit((LSLLabel *)node);
        case NODE_JUMP_STATEMENT:
          return visit((LSLJumpStatement *)node);
        case NODE_IF_STATEMENT:
          return visit((LSLIfStatement *)node);
        case NODE_FOR_STATEMENT:
          return visit((LSLForStatement *)node);
        case NODE_DO_STATEMENT:
          return visit((LSLDoStatement *)node);
        case NODE_WHILE_STATEMENT:
          return visit((LSLWhileStatement *)node);
        case NODE_DECLARATION:
          return visit((LSLDeclaration *)node);
        case NODE_STATE_STATEMENT:
          return visit((LSLStateStatement *)node);
        case NODE_NOP_STATEMENT:
          return visit((LSLNopStatement *)node);
        default:
          return visit((LSLStatement *)node);
      }
    case NODE_EXPRESSION:
      switch(node->getNodeSubType()) {
        case NODE_TYPECAST_EXPRESSION:
          return visit((LSLTypecastExpression *)node);
        case NODE_BOOL_CONVERSION_EXPRESSION:
          return visit((LSLBoolConversionExpression *)node);
        case NODE_PRINT_EXPRESSION:
          return visit((LSLPrintExpression *)node);
        case NODE_FUNCTION_EXPRESSION:
          return visit((LSLFunctionExpression *)node);
        case NODE_VECTOR_EXPRESSION:
          return visit((LSLVectorExpression *)node);
        case NODE_QUATERNION_EXPRESSION:
          return visit((LSLQuaternionExpression *)node);
        case NODE_LIST_EXPRESSION:
          return visit((LSLListExpression *)node);
        case NODE_LVALUE_EXPRESSION:
          return visit((LSLLValueExpression *)node);
        case NODE_PARENTHESIS_EXPRESSION:
          return visit((LSLParenthesisExpression *)node);
        case NODE_BINARY_EXPRESSION:
          return visit((LSLBinaryExpression *)node);
        case NODE_UNARY_EXPRESSION:
          return visit((LSLUnaryExpression *)node);
        case NODE_CONSTANT_EXPRESSION:
          return visit((LSLConstantExpression *)node);
        default:
          return visit((LSLExpression *)node);
      }
    case NODE_TYPE:
      return visit((LSLType *)node);
  }
  return visit(node);
}

void ASTVisitor::visitChildren(LSLASTNode *node) {
  auto child_iter = node->begin();
  auto end = node->end();

  while (child_iter != end) {
    auto *child = *child_iter;
    // increment before visiting, we may swap this node's siblings!
    ++child_iter;
    assert(child != node);
    assert(child);
    child->visit(this);
  }
}

}
