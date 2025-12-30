
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.hpp"
#include "sql/parser/lex_sql.h"
#include "sql/expr/expression.h"

using namespace std;

string token_name(const char *sql_string, YYLTYPE *llocp)
{
  return string(sql_string + llocp->first_column, llocp->last_column - llocp->first_column + 1);
}

int yyerror(YYLTYPE *llocp, const char *sql_string, ParsedSqlResult *sql_result, yyscan_t scanner, const char *msg)
{
  unique_ptr<ParsedSqlNode> error_sql_node = make_unique<ParsedSqlNode>(SCF_ERROR);
  error_sql_node->error.error_msg = msg;
  error_sql_node->error.line = llocp->first_line;
  error_sql_node->error.column = llocp->first_column;
  sql_result->add_sql_node(std::move(error_sql_node));
  return 0;
}

ArithmeticExpr *create_arithmetic_expression(ArithmeticExpr::Type type,
                                             Expression *left,
                                             Expression *right,
                                             const char *sql_string,
                                             YYLTYPE *llocp)
{
  ArithmeticExpr *expr = new ArithmeticExpr(type, left, right);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

UnboundAggregateExpr *create_aggregate_expression(const char *aggregate_name,
                                           Expression *child,
                                           const char *sql_string,
                                           YYLTYPE *llocp)
{
  UnboundAggregateExpr *expr = new UnboundAggregateExpr(aggregate_name, child);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

%}

%define api.pure full
%define parse.error verbose
/** 启用位置标识 **/
%locations
%lex-param { yyscan_t scanner }
/** 这些定义了在yyparse函数中的参数 **/
%parse-param { const char * sql_string }
%parse-param { ParsedSqlResult * sql_result }
%parse-param { void * scanner }

//标识tokens
%token  SEMICOLON
        BY
        CREATE
        DROP
        GROUP
        ORDER
        INNER_JOIN
        TABLE
        TABLES
        INDEX
        CALC
        SELECT
        ASC_T
        DESC_T
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        INT_T
        STRING_T
        FLOAT_T
        DATE_T
        TEXT_T
        VECTOR_T
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
        OR
        SET
        ON
        IN
        NOT_IN
        EXISTS
        NOT_EXISTS
        LOAD
        DATA
        INFILE
        EXPLAIN
        STORAGE
        FORMAT
        IS
        NOT
        NUL
        NULLABLE
        EQ
        LT
        GT
        LE
        GE
        NE
        MAX
        MIN
        COUNT
        AVG
        SUM
        UNIQUE

/** union 中定义各种数据类型，真实生成的代码也是union类型，所以不能有非POD类型的数据 **/
%union {
  ParsedSqlNode *                            sql_node;
  ConditionSqlNode *                         condition;
  Value *                                    value;
  enum CompOp                                comp;
  RelAttrSqlNode *                           rel_attr;
  vector<AttrInfoSqlNode> *             attr_infos;
  AttrInfoSqlNode *                          attr_info;
  Expression *                               expression;
  vector<unique_ptr<Expression>> * expression_list;
  vector<Value> *                       value_list;
  vector<std::vector<Value>> *          multi_value_list; // 新增的字段，用于存储多行VALUES数据
  InsertTuple *                        insert_tuple;
  vector<InsertTuple> *           insert_tuple_list;
  vector<ConditionSqlNode> *            condition_list;
  vector<RelAttrSqlNode> *              rel_attr_list;
  vector<string> *                 relation_list;
  OrderType                         order_type;
  OrderSqlNode *                    order_node;
  std::vector<OrderSqlNode> *       order_list;
  char *                                     cstring;
  int                                        number;
  float                                      floats;
  bool                                       boolean;


  vector<JoinSqlNode>*         join_list;
  std::vector<std::string>*                  ids_list;
}

%token <number> NUMBER
%token <floats> FLOAT
%token <cstring> ID
%token <cstring> SSS
//非终结符

/** type 定义了各种解析后的结果输出的是什么类型。类型对应了 union 中的定义的成员变量名称 **/
%type <number>              type
%type <condition>           condition
%type <value>               value
%type <number>              number
%type <cstring>             relation
%type <comp>                comp_op
%type <rel_attr>            rel_attr
%type <attr_infos>          attr_def_list
%type <attr_info>           attr_def
%type <value_list>          value_list
%type <condition_list>      where
%type <condition_list>      condition_list
%type <cstring>             storage_format

%type <relation_list>       rel_list
%type <expression>          expression
%type <expression_list>     expression_list
%type <expression_list>     group_by
%type <sql_node>            calc_stmt
%type <sql_node>            select_stmt
%type <sql_node>            insert_stmt
%type <insert_tuple>           insert_tuple
%type <insert_tuple_list>      insert_tuple_list
%type <sql_node>            update_stmt
%type <sql_node>            delete_stmt
%type <sql_node>            create_table_stmt
%type <sql_node>            drop_table_stmt
%type <sql_node>            show_tables_stmt
%type <sql_node>            desc_table_stmt
%type <sql_node>            create_index_stmt
%type <sql_node>            drop_index_stmt
%type <sql_node>            sync_stmt
%type <sql_node>            begin_stmt
%type <sql_node>            commit_stmt
%type <sql_node>            rollback_stmt
%type <sql_node>            load_data_stmt
%type <sql_node>            explain_stmt
%type <sql_node>            set_variable_stmt
%type <sql_node>            help_stmt
%type <sql_node>            exit_stmt
%type <sql_node>            command_wrapper
// commands should be a list but I use a single command instead
%type <sql_node>            commands

%type <join_list>           join_list
%type <order_list>          order_by
%type <order_type>          order_type
%type <order_node>          order_node
%type <order_list>          order_list

%type <ids_list>         id_list

%left '+' '-'
%left '*' '/'
%nonassoc UMINUS
%%

commands: command_wrapper opt_semicolon  //commands or sqls. parser starts here.
  {
    unique_ptr<ParsedSqlNode> sql_node = unique_ptr<ParsedSqlNode>($1);
    sql_result->add_sql_node(std::move(sql_node));
  }
  ;

command_wrapper:
    calc_stmt
  | select_stmt
  | insert_stmt
  | update_stmt
  | delete_stmt
  | create_table_stmt
  | drop_table_stmt
  | show_tables_stmt
  | desc_table_stmt
  | create_index_stmt
  | drop_index_stmt
  | sync_stmt
  | begin_stmt
  | commit_stmt
  | rollback_stmt
  | load_data_stmt
  | explain_stmt
  | set_variable_stmt
  | help_stmt
  | exit_stmt
    ;

exit_stmt:      
    EXIT {
      (void)yynerrs;  // 这么写为了消除yynerrs未使用的告警。如果你有更好的方法欢迎提PR
      $$ = new ParsedSqlNode(SCF_EXIT);
    };

help_stmt:
    HELP {
      $$ = new ParsedSqlNode(SCF_HELP);
    };

sync_stmt:
    SYNC {
      $$ = new ParsedSqlNode(SCF_SYNC);
    }
    ;

begin_stmt:
    TRX_BEGIN  {
      $$ = new ParsedSqlNode(SCF_BEGIN);
    }
    ;

commit_stmt:
    TRX_COMMIT {
      $$ = new ParsedSqlNode(SCF_COMMIT);
    }
    ;

rollback_stmt:
    TRX_ROLLBACK  {
      $$ = new ParsedSqlNode(SCF_ROLLBACK);
    }
    ;

drop_table_stmt:    /*drop table 语句的语法解析树*/
    DROP TABLE ID {
      $$ = new ParsedSqlNode(SCF_DROP_TABLE);
      $$->drop_table.relation_name = $3;
      free($3);
    };

show_tables_stmt:
    SHOW TABLES {
      $$ = new ParsedSqlNode(SCF_SHOW_TABLES);
    }
    ;

desc_table_stmt:
    DESC_T ID  {
      $$ = new ParsedSqlNode(SCF_DESC_TABLE);
      $$->desc_table.relation_name = $2;
      free($2);
    }
    ;

create_index_stmt:    /*create index 语句的语法解析树*/
    CREATE INDEX ID ON ID LBRACE id_list RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.index_name = $3;
      create_index.relation_name = $5;
      create_index.attribute_names = *$7;
      create_index.isUnique=false;
      free($3);
      free($5);
      delete($7);
    }
    | CREATE UNIQUE INDEX ID ON ID LBRACE id_list RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.index_name = $4;
      create_index.relation_name = $6;
      create_index.attribute_names = *$8;
      create_index.isUnique=true;
      free($4);
      free($6);
      delete($8);
    }
    ;
  
id_list:
    ID
    {
      $$ = new std::vector<std::string>;
      $$->emplace_back($1);
      free($1);
    }
    | ID COMMA id_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<std::string>;
      }
      $$->emplace_back($1);
      free($1);
    }
    ;

drop_index_stmt:      /*drop index 语句的语法解析树*/
    DROP INDEX ID ON ID
    {
      $$ = new ParsedSqlNode(SCF_DROP_INDEX);
      $$->drop_index.index_name = $3;
      $$->drop_index.relation_name = $5;
      free($3);
      free($5);
    }
    ;
create_table_stmt:    /*create table 语句的语法解析树*/
    CREATE TABLE ID LBRACE attr_def attr_def_list RBRACE storage_format
    {
      $$ = new ParsedSqlNode(SCF_CREATE_TABLE);
      CreateTableSqlNode &create_table = $$->create_table;
      create_table.relation_name = $3;
      free($3);

      vector<AttrInfoSqlNode> *src_attrs = $6;

      if (src_attrs != nullptr) {
        create_table.attr_infos.swap(*src_attrs);
        delete src_attrs;
      }
      create_table.attr_infos.emplace_back(*$5);
      reverse(create_table.attr_infos.begin(), create_table.attr_infos.end());
      delete $5;
      if ($8 != nullptr) {
        create_table.storage_format = $8;
        free($8);
      }
    }
    ;
attr_def_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA attr_def attr_def_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<AttrInfoSqlNode>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
    
attr_def:
    ID type LBRACE number RBRACE 
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable = false;
      free($1);
    }
    | ID type LBRACE number RBRACE NOT NUL
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable = false;
      free($1);
    }
    | ID type LBRACE number RBRACE NULLABLE
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable = true;
      free($1);
    }
    | ID type
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable = false;
      free($1);
    }
    | ID type NOT NUL
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable = false;
      free($1);
    }
    | ID type NULLABLE
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable = true;
      free($1);
    }
    ;
number:
    NUMBER {$$ = $1;}
    ;
type:
    INT_T      { $$ = static_cast<int>(AttrType::INTS); }
    | STRING_T { $$ = static_cast<int>(AttrType::CHARS); }
    | FLOAT_T  { $$ = static_cast<int>(AttrType::FLOATS); }
    | DATE_T  { $$ = static_cast<int>(AttrType::DATES); }
    | TEXT_T  { $$ = static_cast<int>(AttrType::TEXTS); }
    | VECTOR_T { $$ = static_cast<int>(AttrType::VECTORS); }
    ;
insert_stmt:        /*insert   语句的语法解析树*/
    INSERT INTO ID VALUES insert_tuple insert_tuple_list
    {
      $$ = new ParsedSqlNode(SCF_INSERT);
      $$->insertion.relation_name = $3;
      if ($6 != nullptr) {
        $$->insertion.tuples.swap(*$6);
      }
      $$->insertion.tuples.emplace_back(*$5);
      reverse($$->insertion.tuples.begin(), $$->insertion.tuples.end());
      free($3);
      delete($5);
    }
    ;

insert_tuple_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA insert_tuple insert_tuple_list  { 
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<InsertTuple>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
insert_tuple:
    LBRACE value value_list RBRACE {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new InsertTuple;
      }
      $$->emplace_back(*$2);
      std::reverse($$->begin(), $$->end());
      delete $2;
    }
    ;

value_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA value value_list  { 
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<Value>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
value:
    NUMBER {
      $$ = new Value((int)$1);
      @$ = @1;
    }
    |FLOAT {
      $$ = new Value((float)$1);
      @$ = @1;
    }
    |SSS {
      char *tmp = common::substr($1,1,strlen($1)-2);
      $$ = new Value(tmp);
      free(tmp);
      free($1);
    }
    |NUL {
      $$ = new Value("", 0, true);
    }
    ;
storage_format:
    /* empty */
    {
      $$ = nullptr;
    }
    | STORAGE FORMAT EQ ID
    {
      $$ = $4;
    }
    ;
    
delete_stmt:    /*  delete 语句的语法解析树*/
    DELETE FROM ID where 
    {
      $$ = new ParsedSqlNode(SCF_DELETE);
      $$->deletion.relation_name = $3;
      if ($4 != nullptr) {
        $$->deletion.conditions.swap(*$4);
        delete $4;
      }
      free($3);
    }
    ;
update_stmt:      /*  update 语句的语法解析树*/
    UPDATE ID SET ID EQ value where 
    {
      $$ = new ParsedSqlNode(SCF_UPDATE);
      $$->update.relation_name = $2;
      $$->update.attribute_name = $4;
      $$->update.value = *$6;
      if ($7 != nullptr) {
        $$->update.conditions.swap(*$7);
        delete $7;
      }
      free($2);
      free($4);
      delete($6);
    }
    ;
select_stmt:        /*  select 语句的语法解析树*/
    SELECT expression_list FROM rel_list join_list where group_by order_by
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.expressions.swap(*$2);
        delete $2;
      }
      //from
      if ($4 != nullptr) {
        $$->selection.relations.swap(*$4);
        delete $4;
      }
      //where
      if ($6 != nullptr) {
        for (auto &condition : *$6) {
          $$->selection.conditions.push_back(std::move(condition));
        }
        delete $6;
      }
      // join
      if ($5 != nullptr) {
        /* 由于是递归顺序解析的join，需要 reverse */
        std::reverse($5->begin(), $5->end());
        for (auto &join : *$5) {
          $$->selection.relations.push_back(join.relation);
          for (auto &condition : join.conditions) {
            $$->selection.conditions.emplace_back(std::move(condition));
          }
        }
        delete $5; // TODO(Soulter): free test
      }

      if ($7 != nullptr) {
        $$->selection.group_by.swap(*$7);
        delete $7;
      }
      if ($8 != nullptr) {
        $$->selection.order_by.swap(*$8);
        delete $8;
      }
    }
    ;
join_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | INNER_JOIN ID ON condition_list join_list {
      if ($5 != nullptr) {
        $$ = $5;
      } else {
        $$ = new std::vector<JoinSqlNode>;
      }
      JoinSqlNode join;
      join.relation = $2;
      free $2;
      if($4 != nullptr) {
        std::reverse($4->begin(), $4->end());
        for (auto &condition : *$4) {
          join.conditions.emplace_back(std::move(condition));
        }
        delete $4;
      }
      $$->emplace_back(std::move(join));
    }
    ;
calc_stmt:
    CALC expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    ;

expression_list:
    expression
    {
      $$ = new vector<unique_ptr<Expression>>;
      $$->emplace_back($1);
    }
    | expression COMMA expression_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<unique_ptr<Expression>>;
      }
      $$->emplace($$->begin(), $1);
    }
    ;
expression:
    expression '+' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::ADD, $1, $3, sql_string, &@$);
    }
    | expression '-' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::SUB, $1, $3, sql_string, &@$);
    }
    | expression '*' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::MUL, $1, $3, sql_string, &@$);
    }
    | expression '/' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::DIV, $1, $3, sql_string, &@$);
    }
    | LBRACE select_stmt RBRACE {
      $$ = new SubqueryExpr(*$2);
      $$->set_name(token_name(sql_string, &@$));
      // delete $2;
    }
    | LBRACE expression RBRACE {
      $$ = $2;
      $$->set_name(token_name(sql_string, &@$));
    }
    | '-' expression %prec UMINUS {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::NEGATIVE, $2, nullptr, sql_string, &@$);
    }
    | value {
      $$ = new ValueExpr(*$1);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | LBRACE value value_list RBRACE  {
      std::vector<Value> *values = $3;
      values->emplace_back(*$2);
      $$ = new ValueListExpr(*values);
      $$->set_name(token_name(sql_string, &@$));
    }
    | rel_attr {
      RelAttrSqlNode *node = $1;
      $$ = new UnboundFieldExpr(node->relation_name, node->attribute_name);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | '*' {
      $$ = new StarExpr();
    }
    | MAX LBRACE RBRACE {
      $$ = create_aggregate_expression("MAX", nullptr, sql_string, &@$);
    }
    | SUM LBRACE RBRACE {
      $$ = create_aggregate_expression("SUM", nullptr, sql_string, &@$);
    }
    | MIN LBRACE RBRACE {
      $$ = create_aggregate_expression("MIN", nullptr, sql_string, &@$);
    }
    | AVG LBRACE RBRACE {
      $$ = create_aggregate_expression("AVG", nullptr, sql_string, &@$);
    }
    | COUNT LBRACE RBRACE {
      $$ = create_aggregate_expression("COUNT", nullptr, sql_string, &@$);
    }
    | MAX LBRACE expression RBRACE {
      $$ = create_aggregate_expression("MAX", $3, sql_string, &@$);
    }
    | SUM LBRACE expression RBRACE {
      $$ = create_aggregate_expression("SUM", $3, sql_string, &@$);
    }
    | MIN LBRACE expression RBRACE {
      $$ = create_aggregate_expression("MIN", $3, sql_string, &@$);
    }
    | AVG LBRACE expression RBRACE {
      $$ = create_aggregate_expression("AVG", $3, sql_string, &@$);
    }
    | COUNT LBRACE expression RBRACE {
      $$ = create_aggregate_expression("COUNT", $3, sql_string, &@$);
    }
    | MAX LBRACE expression_list RBRACE {
      $$ = create_aggregate_expression("MAX", nullptr, sql_string, &@$);
    }
    | SUM LBRACE expression_list RBRACE {
      $$ = create_aggregate_expression("SUM", nullptr, sql_string, &@$);
    }
    | MIN LBRACE expression_list RBRACE {
      $$ = create_aggregate_expression("MIN", nullptr, sql_string, &@$);
    }
    | AVG LBRACE expression_list RBRACE {
      $$ = create_aggregate_expression("AVG", nullptr, sql_string, &@$);
    }
    | COUNT LBRACE expression_list RBRACE {
      $$ = create_aggregate_expression("COUNT", nullptr, sql_string, &@$);
    }
    // your code here
    ;

rel_attr:
    ID {
      $$ = new RelAttrSqlNode;
      $$->attribute_name = $1;
      free($1);
    }
    | ID DOT ID {
      $$ = new RelAttrSqlNode;
      $$->relation_name  = $1;
      $$->attribute_name = $3;
      free($1);
      free($3);
    }
    ;

relation:
    ID {
      $$ = $1;
    }
    ;

rel_list:
    relation {
      $$ = new vector<string>();
      $$->push_back($1);
      free($1);
    }
    | relation COMMA rel_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<string>;
      }

      $$->insert($$->begin(), $1);
      free($1);
    }
    ;

where:
    /* empty */
    {
      $$ = nullptr;
    }
    | WHERE condition_list {
      $$ = $2;  
    }
    ;
condition_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | condition {
      $$ = new vector<ConditionSqlNode>;
      $1->conjunction_type = 0;
      $$->push_back(std::move(*$1));
      delete $1;
    }
    | condition AND condition_list {
      $$ = $3;
      $1->conjunction_type = 1;
      $$->push_back(std::move(*$1));
      delete $1;
    }
    | condition OR condition_list {
      $$ = $3;
      $1->conjunction_type = 2;
      $$->push_back(std::move(*$1));
      delete $1;
    }
    ;
condition:
    expression comp_op expression
    {
      $$ = new ConditionSqlNode;
      $$->left_expr = std::unique_ptr<Expression>($1);
      $$->right_expr = std::unique_ptr<Expression>($3);
      $$->comp_op = $2;
    }
    // 懒得之后再判断左 expression 是否为空了，直接在这里加上 EXISTS 吧。
    | EXISTS expression
    {
      $$ = new ConditionSqlNode;
      $$->comp_op = CompOp::EXISTS;
      // left_expr: SpecialPlaceholderExpr
      $$->left_expr = std::make_unique<SpecialPlaceholderExpr>();
      $$->right_expr = std::unique_ptr<Expression>($2);
    }
    | NOT_EXISTS expression
    {
      $$ = new ConditionSqlNode;
      $$->comp_op = CompOp::NOT_EXISTS;
      $$->left_expr = std::make_unique<SpecialPlaceholderExpr>();
      $$->right_expr = std::unique_ptr<Expression>($2);
    }
    ;

comp_op:
      EQ { $$ = CompOp::EQUAL_TO; }
    | LT { $$ = CompOp::LESS_THAN; }
    | GT { $$ = CompOp::GREAT_THAN; }
    | LE { $$ = CompOp::LESS_EQUAL; }
    | GE { $$ = CompOp::GREAT_EQUAL; }
    | NE { $$ = CompOp::NOT_EQUAL; }
    | IS NOT { $$ = CompOp::COMP_IS_NOT; }
    | IS { $$ = CompOp::COMP_IS; }
    | IN { $$ = CompOp::IN; }
    | NOT_IN { $$ = CompOp::NOT_IN; }
    | EXISTS { $$ = CompOp::EXISTS; }
    | NOT_EXISTS { $$ = CompOp::NOT_EXISTS; }
    ;

// your code here
order_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | ORDER BY order_list
    {
      $$ = $3;
      std::reverse($$->begin(), $$->end());
    }
    ;
order_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | order_node 
    {
      $$ = new std::vector<OrderSqlNode>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | order_node COMMA order_list 
    {
      $$ = $3;
      $$->emplace_back(*$1);
      delete $1;
    }
    ;
order_node:
    rel_attr order_type
    {
      $$ = new OrderSqlNode;
      $$->type=$2;
      $$->attribute=*$1;
      delete $1;
    }
    ;
order_type:
    /* empty */
    {
      $$ = ASC;
    }
    | ASC_T 
    {
      $$ = ASC;
    }
    | DESC_T 
    {
      $$ = DESC;
    }
    ;

group_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | GROUP BY expression_list
    {
      $$ = new std::vector<std::unique_ptr<Expression>>;
      $$->swap(*$3);
      delete $3;
    }
    ;
load_data_stmt:
    LOAD DATA INFILE SSS INTO TABLE ID 
    {
      char *tmp_file_name = common::substr($4, 1, strlen($4) - 2);
      
      $$ = new ParsedSqlNode(SCF_LOAD_DATA);
      $$->load_data.relation_name = $7;
      $$->load_data.file_name = tmp_file_name;
      free($7);
      free(tmp_file_name);
    }
    ;

explain_stmt:
    EXPLAIN command_wrapper
    {
      $$ = new ParsedSqlNode(SCF_EXPLAIN);
      $$->explain.sql_node = unique_ptr<ParsedSqlNode>($2);
    }
    ;

set_variable_stmt:
    SET ID EQ value
    {
      $$ = new ParsedSqlNode(SCF_SET_VARIABLE);
      $$->set_variable.name  = $2;
      $$->set_variable.value = *$4;
      free($2);
      delete $4;
    }
    ;

opt_semicolon: /*empty*/
    | SEMICOLON
    ;
%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, ParsedSqlResult *sql_result) {
  yyscan_t scanner;
  yylex_init(&scanner);
  scan_string(s, scanner);
  int result = yyparse(s, sql_result, scanner);
  yylex_destroy(scanner);
  return result;
}
