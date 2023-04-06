/******************************************************************************/
/* This file is generated by the bin/template script and should not be        */
/* modified manually. See                                                     */
/* bin/templates/include/yarp/ast.h.erb                                       */
/* if you are looking to modify the                                           */
/* template                                                                   */
/******************************************************************************/
#ifndef YARP_AST_H
#define YARP_AST_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "yarp/util/yp_string.h"

// This enum represents every type of token in the Ruby source.
typedef enum yp_token_type {
  YP_TOKEN_EOF = 1, // final token in the file
  YP_TOKEN_MISSING, // a token that was expected but not found
  YP_TOKEN_NOT_PROVIDED, // a token that was not present but it is okay
  YP_TOKEN_AMPERSAND, // &
  YP_TOKEN_AMPERSAND_AMPERSAND, // &&
  YP_TOKEN_AMPERSAND_AMPERSAND_EQUAL, // &&=
  YP_TOKEN_AMPERSAND_DOT, // &.
  YP_TOKEN_AMPERSAND_EQUAL, // &=
  YP_TOKEN_BACKTICK, // `
  YP_TOKEN_BACK_REFERENCE, // a back reference
  YP_TOKEN_BANG, // ! or !@
  YP_TOKEN_BANG_EQUAL, // !=
  YP_TOKEN_BANG_TILDE, // !~
  YP_TOKEN_BRACE_LEFT, // {
  YP_TOKEN_BRACE_RIGHT, // }
  YP_TOKEN_BRACKET_LEFT, // [
  YP_TOKEN_BRACKET_LEFT_ARRAY, // [ for the beginning of an array
  YP_TOKEN_BRACKET_LEFT_RIGHT, // []
  YP_TOKEN_BRACKET_LEFT_RIGHT_EQUAL, // []=
  YP_TOKEN_BRACKET_RIGHT, // ]
  YP_TOKEN_CARET, // ^
  YP_TOKEN_CARET_EQUAL, // ^=
  YP_TOKEN_CHARACTER_LITERAL, // a character literal
  YP_TOKEN_CLASS_VARIABLE, // a class variable
  YP_TOKEN_COLON, // :
  YP_TOKEN_COLON_COLON, // ::
  YP_TOKEN_COMMA, // ,
  YP_TOKEN_COMMENT, // a comment
  YP_TOKEN_CONSTANT, // a constant
  YP_TOKEN_DOT, // .
  YP_TOKEN_DOT_DOT, // ..
  YP_TOKEN_DOT_DOT_DOT, // ...
  YP_TOKEN_EMBDOC_BEGIN, // =begin
  YP_TOKEN_EMBDOC_END, // =end
  YP_TOKEN_EMBDOC_LINE, // a line inside of embedded documentation
  YP_TOKEN_EMBEXPR_BEGIN, // #{
  YP_TOKEN_EMBEXPR_END, // }
  YP_TOKEN_EMBVAR, // #
  YP_TOKEN_EQUAL, // =
  YP_TOKEN_EQUAL_EQUAL, // ==
  YP_TOKEN_EQUAL_EQUAL_EQUAL, // ===
  YP_TOKEN_EQUAL_GREATER, // =>
  YP_TOKEN_EQUAL_TILDE, // =~
  YP_TOKEN_FLOAT, // a floating point number
  YP_TOKEN_GLOBAL_VARIABLE, // a global variable
  YP_TOKEN_GREATER, // >
  YP_TOKEN_GREATER_EQUAL, // >=
  YP_TOKEN_GREATER_GREATER, // >>
  YP_TOKEN_GREATER_GREATER_EQUAL, // >>=
  YP_TOKEN_HEREDOC_END, // the end of a heredoc
  YP_TOKEN_HEREDOC_START, // the start of a heredoc
  YP_TOKEN_IDENTIFIER, // an identifier
  YP_TOKEN_IGNORED_NEWLINE, // an ignored newline
  YP_TOKEN_IMAGINARY_NUMBER, // an imaginary number literal
  YP_TOKEN_INSTANCE_VARIABLE, // an instance variable
  YP_TOKEN_INTEGER, // an integer (any base)
  YP_TOKEN_KEYWORD_ALIAS, // alias
  YP_TOKEN_KEYWORD_AND, // and
  YP_TOKEN_KEYWORD_BEGIN, // begin
  YP_TOKEN_KEYWORD_BEGIN_UPCASE, // BEGIN
  YP_TOKEN_KEYWORD_BREAK, // break
  YP_TOKEN_KEYWORD_CASE, // case
  YP_TOKEN_KEYWORD_CLASS, // class
  YP_TOKEN_KEYWORD_DEF, // def
  YP_TOKEN_KEYWORD_DEFINED, // defined?
  YP_TOKEN_KEYWORD_DO, // do
  YP_TOKEN_KEYWORD_DO_LOOP, // do keyword for a predicate in a while, until, or for loop
  YP_TOKEN_KEYWORD_ELSE, // else
  YP_TOKEN_KEYWORD_ELSIF, // elsif
  YP_TOKEN_KEYWORD_END, // end
  YP_TOKEN_KEYWORD_END_UPCASE, // END
  YP_TOKEN_KEYWORD_ENSURE, // ensure
  YP_TOKEN_KEYWORD_FALSE, // false
  YP_TOKEN_KEYWORD_FOR, // for
  YP_TOKEN_KEYWORD_IF, // if
  YP_TOKEN_KEYWORD_IF_MODIFIER, // if in the modifier form
  YP_TOKEN_KEYWORD_IN, // in
  YP_TOKEN_KEYWORD_MODULE, // module
  YP_TOKEN_KEYWORD_NEXT, // next
  YP_TOKEN_KEYWORD_NIL, // nil
  YP_TOKEN_KEYWORD_NOT, // not
  YP_TOKEN_KEYWORD_OR, // or
  YP_TOKEN_KEYWORD_REDO, // redo
  YP_TOKEN_KEYWORD_RESCUE, // rescue
  YP_TOKEN_KEYWORD_RESCUE_MODIFIER, // rescue in the modifier form
  YP_TOKEN_KEYWORD_RETRY, // retry
  YP_TOKEN_KEYWORD_RETURN, // return
  YP_TOKEN_KEYWORD_SELF, // self
  YP_TOKEN_KEYWORD_SUPER, // super
  YP_TOKEN_KEYWORD_THEN, // then
  YP_TOKEN_KEYWORD_TRUE, // true
  YP_TOKEN_KEYWORD_UNDEF, // undef
  YP_TOKEN_KEYWORD_UNLESS, // unless
  YP_TOKEN_KEYWORD_UNLESS_MODIFIER, // unless in the modifier form
  YP_TOKEN_KEYWORD_UNTIL, // until
  YP_TOKEN_KEYWORD_UNTIL_MODIFIER, // until in the modifier form
  YP_TOKEN_KEYWORD_WHEN, // when
  YP_TOKEN_KEYWORD_WHILE, // while
  YP_TOKEN_KEYWORD_WHILE_MODIFIER, // while in the modifier form
  YP_TOKEN_KEYWORD_YIELD, // yield
  YP_TOKEN_KEYWORD___ENCODING__, // __ENCODING__
  YP_TOKEN_KEYWORD___FILE__, // __FILE__
  YP_TOKEN_KEYWORD___LINE__, // __LINE__
  YP_TOKEN_LABEL, // a label
  YP_TOKEN_LABEL_END, // the end of a label
  YP_TOKEN_LAMBDA_BEGIN, // {
  YP_TOKEN_LESS, // <
  YP_TOKEN_LESS_EQUAL, // <=
  YP_TOKEN_LESS_EQUAL_GREATER, // <=>
  YP_TOKEN_LESS_LESS, // <<
  YP_TOKEN_LESS_LESS_EQUAL, // <<=
  YP_TOKEN_MINUS, // -
  YP_TOKEN_MINUS_EQUAL, // -=
  YP_TOKEN_MINUS_GREATER, // ->
  YP_TOKEN_NEWLINE, // a newline character outside of other tokens
  YP_TOKEN_NTH_REFERENCE, // an nth global variable reference
  YP_TOKEN_PARENTHESIS_LEFT, // (
  YP_TOKEN_PARENTHESIS_LEFT_PARENTHESES, // ( for a parentheses node
  YP_TOKEN_PARENTHESIS_RIGHT, // )
  YP_TOKEN_PERCENT, // %
  YP_TOKEN_PERCENT_EQUAL, // %=
  YP_TOKEN_PERCENT_LOWER_I, // %i
  YP_TOKEN_PERCENT_LOWER_W, // %w
  YP_TOKEN_PERCENT_LOWER_X, // %x
  YP_TOKEN_PERCENT_UPPER_I, // %I
  YP_TOKEN_PERCENT_UPPER_W, // %W
  YP_TOKEN_PIPE, // |
  YP_TOKEN_PIPE_EQUAL, // |=
  YP_TOKEN_PIPE_PIPE, // ||
  YP_TOKEN_PIPE_PIPE_EQUAL, // ||=
  YP_TOKEN_PLUS, // +
  YP_TOKEN_PLUS_EQUAL, // +=
  YP_TOKEN_QUESTION_MARK, // ?
  YP_TOKEN_RATIONAL_NUMBER, // a rational number literal
  YP_TOKEN_REGEXP_BEGIN, // the beginning of a regular expression
  YP_TOKEN_REGEXP_END, // the end of a regular expression
  YP_TOKEN_SEMICOLON, // ;
  YP_TOKEN_SLASH, // /
  YP_TOKEN_SLASH_EQUAL, // /=
  YP_TOKEN_STAR, // *
  YP_TOKEN_STAR_EQUAL, // *=
  YP_TOKEN_STAR_STAR, // **
  YP_TOKEN_STAR_STAR_EQUAL, // **=
  YP_TOKEN_STRING_BEGIN, // the beginning of a string
  YP_TOKEN_STRING_CONTENT, // the contents of a string
  YP_TOKEN_STRING_END, // the end of a string
  YP_TOKEN_SYMBOL_BEGIN, // the beginning of a symbol
  YP_TOKEN_TILDE, // ~ or ~@
  YP_TOKEN_UCOLON_COLON, // unary ::
  YP_TOKEN_UDOT_DOT, // unary ..
  YP_TOKEN_UDOT_DOT_DOT, // unary ...
  YP_TOKEN_UMINUS, // -@
  YP_TOKEN_UMINUS_NUM, // -@ for a number
  YP_TOKEN_UPLUS, // +@
  YP_TOKEN_USTAR, // unary *
  YP_TOKEN_USTAR_STAR, // unary **
  YP_TOKEN_WORDS_SEP, // a separator between words in a list
  YP_TOKEN___END__, // marker for the point in the file at which the parser should stop
  YP_TOKEN_MAXIMUM, // the maximum token value
} yp_token_type_t;

// This struct represents a token in the Ruby source. We use it to track both
// type and location information.
typedef struct {
  yp_token_type_t type;
  const char *start;
  const char *end;
} yp_token_t;

typedef struct {
  yp_token_t *tokens;
  size_t size;
  size_t capacity;
} yp_token_list_t;

struct yp_node;

typedef struct yp_node_list {
  struct yp_node **nodes;
  size_t size;
  size_t capacity;
} yp_node_list_t;

typedef enum {
  YP_NODE_ALIAS_NODE = 1,
  YP_NODE_ALTERNATION_PATTERN_NODE = 2,
  YP_NODE_AND_NODE = 3,
  YP_NODE_ARGUMENTS_NODE = 4,
  YP_NODE_ARRAY_NODE = 5,
  YP_NODE_ARRAY_PATTERN_NODE = 6,
  YP_NODE_ASSOC_NODE = 7,
  YP_NODE_ASSOC_SPLAT_NODE = 8,
  YP_NODE_BEGIN_NODE = 9,
  YP_NODE_BLOCK_ARGUMENT_NODE = 10,
  YP_NODE_BLOCK_NODE = 11,
  YP_NODE_BLOCK_PARAMETER_NODE = 12,
  YP_NODE_BLOCK_PARAMETERS_NODE = 13,
  YP_NODE_BREAK_NODE = 14,
  YP_NODE_CALL_NODE = 15,
  YP_NODE_CAPTURE_PATTERN_NODE = 16,
  YP_NODE_CASE_NODE = 17,
  YP_NODE_CLASS_NODE = 18,
  YP_NODE_CLASS_VARIABLE_READ_NODE = 19,
  YP_NODE_CLASS_VARIABLE_WRITE_NODE = 20,
  YP_NODE_CONSTANT_PATH_NODE = 21,
  YP_NODE_CONSTANT_PATH_WRITE_NODE = 22,
  YP_NODE_CONSTANT_READ_NODE = 23,
  YP_NODE_DEF_NODE = 24,
  YP_NODE_DEFINED_NODE = 25,
  YP_NODE_ELSE_NODE = 26,
  YP_NODE_ENSURE_NODE = 27,
  YP_NODE_FALSE_NODE = 28,
  YP_NODE_FIND_PATTERN_NODE = 29,
  YP_NODE_FLOAT_NODE = 30,
  YP_NODE_FOR_NODE = 31,
  YP_NODE_FORWARDING_ARGUMENTS_NODE = 32,
  YP_NODE_FORWARDING_PARAMETER_NODE = 33,
  YP_NODE_FORWARDING_SUPER_NODE = 34,
  YP_NODE_GLOBAL_VARIABLE_READ_NODE = 35,
  YP_NODE_GLOBAL_VARIABLE_WRITE_NODE = 36,
  YP_NODE_HASH_NODE = 37,
  YP_NODE_HASH_PATTERN_NODE = 38,
  YP_NODE_IF_NODE = 39,
  YP_NODE_IMAGINARY_NODE = 40,
  YP_NODE_IN_NODE = 41,
  YP_NODE_INSTANCE_VARIABLE_READ_NODE = 42,
  YP_NODE_INSTANCE_VARIABLE_WRITE_NODE = 43,
  YP_NODE_INTEGER_NODE = 44,
  YP_NODE_INTERPOLATED_REGULAR_EXPRESSION_NODE = 45,
  YP_NODE_INTERPOLATED_STRING_NODE = 46,
  YP_NODE_INTERPOLATED_SYMBOL_NODE = 47,
  YP_NODE_INTERPOLATED_X_STRING_NODE = 48,
  YP_NODE_KEYWORD_PARAMETER_NODE = 49,
  YP_NODE_KEYWORD_REST_PARAMETER_NODE = 50,
  YP_NODE_LAMBDA_NODE = 51,
  YP_NODE_LOCAL_VARIABLE_READ_NODE = 52,
  YP_NODE_LOCAL_VARIABLE_WRITE_NODE = 53,
  YP_NODE_MATCH_PREDICATE_NODE = 54,
  YP_NODE_MATCH_REQUIRED_NODE = 55,
  YP_NODE_MISSING_NODE = 56,
  YP_NODE_MODULE_NODE = 57,
  YP_NODE_MULTI_WRITE_NODE = 58,
  YP_NODE_NEXT_NODE = 59,
  YP_NODE_NIL_NODE = 60,
  YP_NODE_NO_KEYWORDS_PARAMETER_NODE = 61,
  YP_NODE_OPERATOR_AND_ASSIGNMENT_NODE = 62,
  YP_NODE_OPERATOR_ASSIGNMENT_NODE = 63,
  YP_NODE_OPERATOR_OR_ASSIGNMENT_NODE = 64,
  YP_NODE_OPTIONAL_PARAMETER_NODE = 65,
  YP_NODE_OR_NODE = 66,
  YP_NODE_PARAMETERS_NODE = 67,
  YP_NODE_PARENTHESES_NODE = 68,
  YP_NODE_PINNED_EXPRESSION_NODE = 69,
  YP_NODE_PINNED_VARIABLE_NODE = 70,
  YP_NODE_POST_EXECUTION_NODE = 71,
  YP_NODE_PRE_EXECUTION_NODE = 72,
  YP_NODE_PROGRAM_NODE = 73,
  YP_NODE_RANGE_NODE = 74,
  YP_NODE_RATIONAL_NODE = 75,
  YP_NODE_REDO_NODE = 76,
  YP_NODE_REGULAR_EXPRESSION_NODE = 77,
  YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE = 78,
  YP_NODE_REQUIRED_PARAMETER_NODE = 79,
  YP_NODE_RESCUE_MODIFIER_NODE = 80,
  YP_NODE_RESCUE_NODE = 81,
  YP_NODE_REST_PARAMETER_NODE = 82,
  YP_NODE_RETRY_NODE = 83,
  YP_NODE_RETURN_NODE = 84,
  YP_NODE_SCOPE_NODE = 85,
  YP_NODE_SELF_NODE = 86,
  YP_NODE_SINGLETON_CLASS_NODE = 87,
  YP_NODE_SOURCE_ENCODING_NODE = 88,
  YP_NODE_SOURCE_FILE_NODE = 89,
  YP_NODE_SOURCE_LINE_NODE = 90,
  YP_NODE_SPLAT_NODE = 91,
  YP_NODE_STATEMENTS_NODE = 92,
  YP_NODE_STRING_CONCAT_NODE = 93,
  YP_NODE_STRING_INTERPOLATED_NODE = 94,
  YP_NODE_STRING_NODE = 95,
  YP_NODE_SUPER_NODE = 96,
  YP_NODE_SYMBOL_NODE = 97,
  YP_NODE_TRUE_NODE = 98,
  YP_NODE_UNDEF_NODE = 99,
  YP_NODE_UNLESS_NODE = 100,
  YP_NODE_UNTIL_NODE = 101,
  YP_NODE_WHEN_NODE = 102,
  YP_NODE_WHILE_NODE = 103,
  YP_NODE_X_STRING_NODE = 104,
  YP_NODE_YIELD_NODE = 105,
} yp_node_type_t;

// This represents a range of bytes in the source string to which a node or
// token corresponds.
typedef struct {
  const char *start;
  const char *end;
} yp_location_t;

// This is the overall tagged union representing a node in the syntax tree.
typedef struct yp_node {
  // This represents the type of the node. It somewhat maps to the nodes that
  // existed in the original grammar and ripper, but it's not a 1:1 mapping.
  yp_node_type_t type;

  // This is the location of the node in the source. It's a range of bytes
  // containing a start and an end.
  yp_location_t location;
} yp_node_t;

// AliasNode
typedef struct yp_alias_node {
  yp_node_t base;
  struct yp_node *new_name;
  struct yp_node *old_name;
  yp_location_t keyword_loc;
} yp_alias_node_t;

// AlternationPatternNode
typedef struct yp_alternation_pattern_node {
  yp_node_t base;
  struct yp_node *left;
  struct yp_node *right;
  yp_location_t operator_loc;
} yp_alternation_pattern_node_t;

// AndNode
typedef struct yp_and_node {
  yp_node_t base;
  struct yp_node *left;
  struct yp_node *right;
  yp_token_t operator;
} yp_and_node_t;

// ArgumentsNode
typedef struct yp_arguments_node {
  yp_node_t base;
  struct yp_node_list arguments;
} yp_arguments_node_t;

// ArrayNode
typedef struct yp_array_node {
  yp_node_t base;
  struct yp_node_list elements;
  yp_token_t opening;
  yp_token_t closing;
} yp_array_node_t;

// ArrayPatternNode
typedef struct yp_array_pattern_node {
  yp_node_t base;
  struct yp_node *constant;
  struct yp_node_list requireds;
  struct yp_node *rest;
  struct yp_node_list posts;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_array_pattern_node_t;

// AssocNode
typedef struct yp_assoc_node {
  yp_node_t base;
  struct yp_node *key;
  struct yp_node *value;
  yp_token_t operator;
} yp_assoc_node_t;

// AssocSplatNode
typedef struct yp_assoc_splat_node {
  yp_node_t base;
  struct yp_node *value;
  yp_location_t operator_loc;
} yp_assoc_splat_node_t;

// BeginNode
typedef struct yp_begin_node {
  yp_node_t base;
  yp_token_t begin_keyword;
  struct yp_statements_node *statements;
  struct yp_rescue_node *rescue_clause;
  struct yp_else_node *else_clause;
  struct yp_ensure_node *ensure_clause;
  yp_token_t end_keyword;
} yp_begin_node_t;

// BlockArgumentNode
typedef struct yp_block_argument_node {
  yp_node_t base;
  struct yp_node *expression;
  yp_location_t operator_loc;
} yp_block_argument_node_t;

// BlockNode
typedef struct yp_block_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  struct yp_block_parameters_node *parameters;
  struct yp_node *statements;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_block_node_t;

// BlockParameterNode
typedef struct yp_block_parameter_node {
  yp_node_t base;
  yp_token_t name;
  yp_location_t operator_loc;
} yp_block_parameter_node_t;

// BlockParametersNode
typedef struct yp_block_parameters_node {
  yp_node_t base;
  struct yp_parameters_node *parameters;
  yp_token_list_t locals;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_block_parameters_node_t;

// BreakNode
typedef struct yp_break_node {
  yp_node_t base;
  struct yp_arguments_node *arguments;
  yp_location_t keyword_loc;
} yp_break_node_t;

// CallNode
typedef struct yp_call_node {
  yp_node_t base;
  struct yp_node *receiver;
  yp_token_t call_operator;
  yp_token_t message;
  yp_token_t opening;
  struct yp_arguments_node *arguments;
  yp_token_t closing;
  struct yp_block_node *block;
  yp_string_t name;
} yp_call_node_t;

// CapturePatternNode
typedef struct yp_capture_pattern_node {
  yp_node_t base;
  struct yp_node *value;
  struct yp_node *target;
  yp_location_t operator_loc;
} yp_capture_pattern_node_t;

// CaseNode
typedef struct yp_case_node {
  yp_node_t base;
  struct yp_node *predicate;
  struct yp_node_list conditions;
  struct yp_else_node *consequent;
  yp_location_t case_keyword_loc;
  yp_location_t end_keyword_loc;
} yp_case_node_t;

// ClassNode
typedef struct yp_class_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  yp_token_t class_keyword;
  struct yp_node *constant_path;
  yp_token_t inheritance_operator;
  struct yp_node *superclass;
  struct yp_node *statements;
  yp_token_t end_keyword;
} yp_class_node_t;

// ClassVariableReadNode
typedef struct yp_class_variable_read_node {
  yp_node_t base;
} yp_class_variable_read_node_t;

// ClassVariableWriteNode
typedef struct yp_class_variable_write_node {
  yp_node_t base;
  yp_location_t name_loc;
  struct yp_node *value;
  yp_location_t operator_loc;
} yp_class_variable_write_node_t;

// ConstantPathNode
typedef struct yp_constant_path_node {
  yp_node_t base;
  struct yp_node *parent;
  struct yp_node *child;
  yp_location_t delimiter_loc;
} yp_constant_path_node_t;

// ConstantPathWriteNode
typedef struct yp_constant_path_write_node {
  yp_node_t base;
  struct yp_node *target;
  yp_token_t operator;
  struct yp_node *value;
} yp_constant_path_write_node_t;

// ConstantReadNode
typedef struct yp_constant_read_node {
  yp_node_t base;
} yp_constant_read_node_t;

// DefNode
typedef struct yp_def_node {
  yp_node_t base;
  yp_token_t name;
  struct yp_node *receiver;
  struct yp_parameters_node *parameters;
  struct yp_node *statements;
  struct yp_scope_node *scope;
  yp_location_t def_keyword_loc;
  yp_location_t operator_loc;
  yp_location_t lparen_loc;
  yp_location_t rparen_loc;
  yp_location_t equal_loc;
  yp_location_t end_keyword_loc;
} yp_def_node_t;

// DefinedNode
typedef struct yp_defined_node {
  yp_node_t base;
  yp_token_t lparen;
  struct yp_node *value;
  yp_token_t rparen;
  yp_location_t keyword_loc;
} yp_defined_node_t;

// ElseNode
typedef struct yp_else_node {
  yp_node_t base;
  yp_token_t else_keyword;
  struct yp_statements_node *statements;
  yp_token_t end_keyword;
} yp_else_node_t;

// EnsureNode
typedef struct yp_ensure_node {
  yp_node_t base;
  yp_token_t ensure_keyword;
  struct yp_statements_node *statements;
  yp_token_t end_keyword;
} yp_ensure_node_t;

// FalseNode
typedef struct yp_false_node {
  yp_node_t base;
} yp_false_node_t;

// FindPatternNode
typedef struct yp_find_pattern_node {
  yp_node_t base;
  struct yp_node *constant;
  struct yp_node *left;
  struct yp_node_list requireds;
  struct yp_node *right;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_find_pattern_node_t;

// FloatNode
typedef struct yp_float_node {
  yp_node_t base;
} yp_float_node_t;

// ForNode
typedef struct yp_for_node {
  yp_node_t base;
  struct yp_node *index;
  struct yp_node *collection;
  struct yp_statements_node *statements;
  yp_location_t for_keyword_loc;
  yp_location_t in_keyword_loc;
  yp_location_t do_keyword_loc;
  yp_location_t end_keyword_loc;
} yp_for_node_t;

// ForwardingArgumentsNode
typedef struct yp_forwarding_arguments_node {
  yp_node_t base;
} yp_forwarding_arguments_node_t;

// ForwardingParameterNode
typedef struct yp_forwarding_parameter_node {
  yp_node_t base;
} yp_forwarding_parameter_node_t;

// ForwardingSuperNode
typedef struct yp_forwarding_super_node {
  yp_node_t base;
  struct yp_block_node *block;
} yp_forwarding_super_node_t;

// GlobalVariableReadNode
typedef struct yp_global_variable_read_node {
  yp_node_t base;
  yp_token_t name;
} yp_global_variable_read_node_t;

// GlobalVariableWriteNode
typedef struct yp_global_variable_write_node {
  yp_node_t base;
  yp_token_t name;
  yp_token_t operator;
  struct yp_node *value;
} yp_global_variable_write_node_t;

// HashNode
typedef struct yp_hash_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_node_list elements;
  yp_token_t closing;
} yp_hash_node_t;

// HashPatternNode
typedef struct yp_hash_pattern_node {
  yp_node_t base;
  struct yp_node *constant;
  struct yp_node_list assocs;
  struct yp_node *kwrest;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_hash_pattern_node_t;

// IfNode
typedef struct yp_if_node {
  yp_node_t base;
  yp_token_t if_keyword;
  struct yp_node *predicate;
  struct yp_statements_node *statements;
  struct yp_node *consequent;
  yp_token_t end_keyword;
} yp_if_node_t;

// ImaginaryNode
typedef struct yp_imaginary_node {
  yp_node_t base;
} yp_imaginary_node_t;

// InNode
typedef struct yp_in_node {
  yp_node_t base;
  struct yp_node *pattern;
  struct yp_statements_node *statements;
  yp_location_t in_loc;
  yp_location_t then_loc;
} yp_in_node_t;

// InstanceVariableReadNode
typedef struct yp_instance_variable_read_node {
  yp_node_t base;
} yp_instance_variable_read_node_t;

// InstanceVariableWriteNode
typedef struct yp_instance_variable_write_node {
  yp_node_t base;
  yp_location_t name_loc;
  struct yp_node *value;
  yp_location_t operator_loc;
} yp_instance_variable_write_node_t;

// IntegerNode
typedef struct yp_integer_node {
  yp_node_t base;
} yp_integer_node_t;

// InterpolatedRegularExpressionNode
typedef struct yp_interpolated_regular_expression_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_node_list parts;
  yp_token_t closing;
} yp_interpolated_regular_expression_node_t;

// InterpolatedStringNode
typedef struct yp_interpolated_string_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_node_list parts;
  yp_token_t closing;
} yp_interpolated_string_node_t;

// InterpolatedSymbolNode
typedef struct yp_interpolated_symbol_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_node_list parts;
  yp_token_t closing;
} yp_interpolated_symbol_node_t;

// InterpolatedXStringNode
typedef struct yp_interpolated_x_string_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_node_list parts;
  yp_token_t closing;
} yp_interpolated_x_string_node_t;

// KeywordParameterNode
typedef struct yp_keyword_parameter_node {
  yp_node_t base;
  yp_token_t name;
  struct yp_node *value;
} yp_keyword_parameter_node_t;

// KeywordRestParameterNode
typedef struct yp_keyword_rest_parameter_node {
  yp_node_t base;
  yp_token_t operator;
  yp_token_t name;
} yp_keyword_rest_parameter_node_t;

// LambdaNode
typedef struct yp_lambda_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  yp_token_t opening;
  struct yp_block_parameters_node *parameters;
  struct yp_node *statements;
} yp_lambda_node_t;

// LocalVariableReadNode
typedef struct yp_local_variable_read_node {
  yp_node_t base;
  int depth;
} yp_local_variable_read_node_t;

// LocalVariableWriteNode
typedef struct yp_local_variable_write_node {
  yp_node_t base;
  yp_location_t name_loc;
  struct yp_node *value;
  yp_location_t operator_loc;
  int depth;
} yp_local_variable_write_node_t;

// MatchPredicateNode
typedef struct yp_match_predicate_node {
  yp_node_t base;
  struct yp_node *value;
  struct yp_node *pattern;
  yp_location_t operator_loc;
} yp_match_predicate_node_t;

// MatchRequiredNode
typedef struct yp_match_required_node {
  yp_node_t base;
  struct yp_node *value;
  struct yp_node *pattern;
  yp_location_t operator_loc;
} yp_match_required_node_t;

// MissingNode
typedef struct yp_missing_node {
  yp_node_t base;
} yp_missing_node_t;

// ModuleNode
typedef struct yp_module_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  yp_token_t module_keyword;
  struct yp_node *constant_path;
  struct yp_node *statements;
  yp_token_t end_keyword;
} yp_module_node_t;

// MultiWriteNode
typedef struct yp_multi_write_node {
  yp_node_t base;
  struct yp_node_list targets;
  yp_token_t operator;
  struct yp_node *value;
  yp_location_t lparen_loc;
  yp_location_t rparen_loc;
} yp_multi_write_node_t;

// NextNode
typedef struct yp_next_node {
  yp_node_t base;
  struct yp_arguments_node *arguments;
  yp_location_t keyword_loc;
} yp_next_node_t;

// NilNode
typedef struct yp_nil_node {
  yp_node_t base;
} yp_nil_node_t;

// NoKeywordsParameterNode
typedef struct yp_no_keywords_parameter_node {
  yp_node_t base;
  yp_location_t operator_loc;
  yp_location_t keyword_loc;
} yp_no_keywords_parameter_node_t;

// OperatorAndAssignmentNode
typedef struct yp_operator_and_assignment_node {
  yp_node_t base;
  struct yp_node *target;
  struct yp_node *value;
  yp_location_t operator_loc;
} yp_operator_and_assignment_node_t;

// OperatorAssignmentNode
typedef struct yp_operator_assignment_node {
  yp_node_t base;
  struct yp_node *target;
  yp_token_t operator;
  struct yp_node *value;
} yp_operator_assignment_node_t;

// OperatorOrAssignmentNode
typedef struct yp_operator_or_assignment_node {
  yp_node_t base;
  struct yp_node *target;
  struct yp_node *value;
  yp_location_t operator_loc;
} yp_operator_or_assignment_node_t;

// OptionalParameterNode
typedef struct yp_optional_parameter_node {
  yp_node_t base;
  yp_token_t name;
  yp_token_t equal_operator;
  struct yp_node *value;
} yp_optional_parameter_node_t;

// OrNode
typedef struct yp_or_node {
  yp_node_t base;
  struct yp_node *left;
  struct yp_node *right;
  yp_location_t operator_loc;
} yp_or_node_t;

// ParametersNode
typedef struct yp_parameters_node {
  yp_node_t base;
  struct yp_node_list requireds;
  struct yp_node_list optionals;
  struct yp_node_list posts;
  struct yp_rest_parameter_node *rest;
  struct yp_node_list keywords;
  struct yp_node *keyword_rest;
  struct yp_block_parameter_node *block;
} yp_parameters_node_t;

// ParenthesesNode
typedef struct yp_parentheses_node {
  yp_node_t base;
  struct yp_node *statements;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_parentheses_node_t;

// PinnedExpressionNode
typedef struct yp_pinned_expression_node {
  yp_node_t base;
  struct yp_node *expression;
  yp_location_t operator_loc;
  yp_location_t lparen_loc;
  yp_location_t rparen_loc;
} yp_pinned_expression_node_t;

// PinnedVariableNode
typedef struct yp_pinned_variable_node {
  yp_node_t base;
  struct yp_node *variable;
  yp_location_t operator_loc;
} yp_pinned_variable_node_t;

// PostExecutionNode
typedef struct yp_post_execution_node {
  yp_node_t base;
  struct yp_statements_node *statements;
  yp_location_t keyword_loc;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_post_execution_node_t;

// PreExecutionNode
typedef struct yp_pre_execution_node {
  yp_node_t base;
  struct yp_statements_node *statements;
  yp_location_t keyword_loc;
  yp_location_t opening_loc;
  yp_location_t closing_loc;
} yp_pre_execution_node_t;

// ProgramNode
typedef struct yp_program_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  struct yp_statements_node *statements;
} yp_program_node_t;

// RangeNode
typedef struct yp_range_node {
  yp_node_t base;
  struct yp_node *left;
  struct yp_node *right;
  yp_location_t operator_loc;
} yp_range_node_t;

// RationalNode
typedef struct yp_rational_node {
  yp_node_t base;
} yp_rational_node_t;

// RedoNode
typedef struct yp_redo_node {
  yp_node_t base;
} yp_redo_node_t;

// RegularExpressionNode
typedef struct yp_regular_expression_node {
  yp_node_t base;
  yp_token_t opening;
  yp_token_t content;
  yp_token_t closing;
  yp_string_t unescaped;
} yp_regular_expression_node_t;

// RequiredDestructuredParameterNode
typedef struct yp_required_destructured_parameter_node {
  yp_node_t base;
  struct yp_node_list parameters;
  yp_token_t opening;
  yp_token_t closing;
} yp_required_destructured_parameter_node_t;

// RequiredParameterNode
typedef struct yp_required_parameter_node {
  yp_node_t base;
} yp_required_parameter_node_t;

// RescueModifierNode
typedef struct yp_rescue_modifier_node {
  yp_node_t base;
  struct yp_node *expression;
  yp_token_t rescue_keyword;
  struct yp_node *rescue_expression;
} yp_rescue_modifier_node_t;

// RescueNode
typedef struct yp_rescue_node {
  yp_node_t base;
  yp_token_t rescue_keyword;
  struct yp_node_list exceptions;
  yp_token_t equal_greater;
  struct yp_node *exception;
  struct yp_statements_node *statements;
  struct yp_rescue_node *consequent;
} yp_rescue_node_t;

// RestParameterNode
typedef struct yp_rest_parameter_node {
  yp_node_t base;
  yp_token_t operator;
  yp_token_t name;
} yp_rest_parameter_node_t;

// RetryNode
typedef struct yp_retry_node {
  yp_node_t base;
} yp_retry_node_t;

// ReturnNode
typedef struct yp_return_node {
  yp_node_t base;
  yp_token_t keyword;
  struct yp_arguments_node *arguments;
} yp_return_node_t;

// ScopeNode
typedef struct yp_scope_node {
  yp_node_t base;
  yp_token_list_t locals;
} yp_scope_node_t;

// SelfNode
typedef struct yp_self_node {
  yp_node_t base;
} yp_self_node_t;

// SingletonClassNode
typedef struct yp_singleton_class_node {
  yp_node_t base;
  struct yp_scope_node *scope;
  yp_token_t class_keyword;
  yp_token_t operator;
  struct yp_node *expression;
  struct yp_node *statements;
  yp_token_t end_keyword;
} yp_singleton_class_node_t;

// SourceEncodingNode
typedef struct yp_source_encoding_node {
  yp_node_t base;
} yp_source_encoding_node_t;

// SourceFileNode
typedef struct yp_source_file_node {
  yp_node_t base;
  yp_string_t filepath;
} yp_source_file_node_t;

// SourceLineNode
typedef struct yp_source_line_node {
  yp_node_t base;
} yp_source_line_node_t;

// SplatNode
typedef struct yp_splat_node {
  yp_node_t base;
  yp_token_t operator;
  struct yp_node *expression;
} yp_splat_node_t;

// StatementsNode
typedef struct yp_statements_node {
  yp_node_t base;
  struct yp_node_list body;
} yp_statements_node_t;

// StringConcatNode
typedef struct yp_string_concat_node {
  yp_node_t base;
  struct yp_node *left;
  struct yp_node *right;
} yp_string_concat_node_t;

// StringInterpolatedNode
typedef struct yp_string_interpolated_node {
  yp_node_t base;
  yp_token_t opening;
  struct yp_statements_node *statements;
  yp_token_t closing;
} yp_string_interpolated_node_t;

// StringNode
typedef struct yp_string_node {
  yp_node_t base;
  yp_token_t opening;
  yp_token_t content;
  yp_token_t closing;
  yp_string_t unescaped;
} yp_string_node_t;

// SuperNode
typedef struct yp_super_node {
  yp_node_t base;
  yp_token_t keyword;
  yp_token_t lparen;
  struct yp_arguments_node *arguments;
  yp_token_t rparen;
  struct yp_block_node *block;
} yp_super_node_t;

// SymbolNode
typedef struct yp_symbol_node {
  yp_node_t base;
  yp_token_t opening;
  yp_token_t value;
  yp_token_t closing;
  yp_string_t unescaped;
} yp_symbol_node_t;

// TrueNode
typedef struct yp_true_node {
  yp_node_t base;
} yp_true_node_t;

// UndefNode
typedef struct yp_undef_node {
  yp_node_t base;
  struct yp_node_list names;
  yp_location_t keyword_loc;
} yp_undef_node_t;

// UnlessNode
typedef struct yp_unless_node {
  yp_node_t base;
  yp_token_t keyword;
  struct yp_node *predicate;
  struct yp_statements_node *statements;
  struct yp_else_node *consequent;
  yp_token_t end_keyword;
} yp_unless_node_t;

// UntilNode
typedef struct yp_until_node {
  yp_node_t base;
  yp_token_t keyword;
  struct yp_node *predicate;
  struct yp_statements_node *statements;
} yp_until_node_t;

// WhenNode
typedef struct yp_when_node {
  yp_node_t base;
  yp_token_t when_keyword;
  struct yp_node_list conditions;
  struct yp_statements_node *statements;
} yp_when_node_t;

// WhileNode
typedef struct yp_while_node {
  yp_node_t base;
  yp_token_t keyword;
  struct yp_node *predicate;
  struct yp_statements_node *statements;
} yp_while_node_t;

// XStringNode
typedef struct yp_x_string_node {
  yp_node_t base;
  yp_token_t opening;
  yp_token_t content;
  yp_token_t closing;
  yp_string_t unescaped;
} yp_x_string_node_t;

// YieldNode
typedef struct yp_yield_node {
  yp_node_t base;
  yp_token_t keyword;
  yp_token_t lparen;
  struct yp_arguments_node *arguments;
  yp_token_t rparen;
} yp_yield_node_t;

#endif // YARP_AST_H
