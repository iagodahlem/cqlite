#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct InputBuffer_t {
  char* buffer;
  size_t buffer_length;
  ssize_t input_length;
};
typedef struct InputBuffer_t InputBuffer;

enum MetaCommandResult_t {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
};
typedef enum MetaCommandResult_t MetaCommandResult;

enum PrepareResult_t {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
  PREPARE_UNRECOGNIZED_STATEMENT
};
typedef enum PrepareResult_t PrepareResult;

enum StatementType_t {
  STATEMENT_INSERT,
  STATEMENT_SELECT
};
typedef enum StatementType_t StatementType;

enum ExecuteResult_t {
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL
};
typedef enum ExecuteResult_t ExecuteResult;

const int COLUMN_KEY_SIZE = 32;
const int COLUMN_VALUE_SIZE = 255;

struct Row_t {
  int id;
  char key[COLUMN_KEY_SIZE];
  char value[COLUMN_VALUE_SIZE];
};
typedef struct Row_t Row;

struct Statement_t {
  StatementType type;
  Row row_to_insert;
};
typedef struct Statement_t Statement;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

const int ID_SIZE = size_of_attribute(Row, id);
const int KEY_SIZE = size_of_attribute(Row, key);
const int VALUE_SIZE = size_of_attribute(Row, value);
const int ID_OFFSET = 0;
const int KEY_OFFSET = ID_OFFSET + ID_SIZE;
const int VALUE_OFFSET = KEY_OFFSET + KEY_SIZE;
const int ROW_SIZE = ID_SIZE + KEY_SIZE + VALUE_SIZE;

const int PAGE_SIZE = 4096;
const int TABLE_MAX_PAGES = 100;
const int ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const int TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

struct Table_t {
  void* pages[TABLE_MAX_PAGES];
  int num_rows;
};
typedef struct Table_t Table;

InputBuffer* new_input_buffer() {
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

Table* new_table() {
  Table* table = malloc(sizeof(Table));
  table->num_rows = 0;

  return table;
}

void print_prompt() {
  printf("db > ");
}

void read_input(InputBuffer* input_buffer) {
  ssize_t bytes_read = getline(&input_buffer->buffer, &input_buffer->buffer_length, stdin);

  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }

  // Ignore trailing newline
  input_buffer->input_length = bytes_read - 1;
  input_buffer->buffer[bytes_read - 1] = 0;
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
  if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    int args_assigned = sscanf(
      input_buffer->buffer,
      "insert %d %s %s",
      &statement->row_to_insert.id,
      statement->row_to_insert.key,
      statement->row_to_insert.value
    );

    if (args_assigned < 3) {
      return PREPARE_SYNTAX_ERROR;
    }

    return PREPARE_SUCCESS;
  }

  if (strcmp(input_buffer->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->key, row->value);
}

void* row_slot(Table* table, int row_num) {
  int page_num = row_num / ROWS_PER_PAGE;
  void* page = table->pages[page_num];
  if (!page) {
    // Allocate memory only when we try to access page
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }
  int row_offset = row_num % ROWS_PER_PAGE;
  int byte_offset = row_offset * ROW_SIZE;
  return page + byte_offset;
}

void serialize_row(Row* source, void* destination) {
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + KEY_OFFSET, &(source->key), KEY_SIZE);
  memcpy(destination + VALUE_OFFSET, &(source->value), VALUE_SIZE);
}

void deserialize_row(void* source, Row* destination) {
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->key), source + KEY_OFFSET, KEY_SIZE);
  memcpy(&(destination->value), source + VALUE_OFFSET, VALUE_SIZE);
}

ExecuteResult execute_insert(Statement* statement, Table* table) {
  if (table->num_rows >= TABLE_MAX_ROWS) {
    return EXECUTE_TABLE_FULL;
  }

  Row* row_to_insert = &(statement->row_to_insert);

  serialize_row(row_to_insert, row_slot(table, table->num_rows));
  table->num_rows += 1;

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
  Row row;
  for (int i = 0; i < table->num_rows; i++) {
    deserialize_row(row_slot(table, i), &row);
    print_row(&row);
  }
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table) {
  switch (statement->type) {
    case STATEMENT_INSERT:
      return execute_insert(statement, table);
    case STATEMENT_SELECT:
      return execute_select(statement, table);
  }
}

int main(int argc, char* argv[]) {
  Table* table = new_table();
  InputBuffer* input_buffer = new_input_buffer();

  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (input_buffer->buffer[0] == '.') {
      switch (do_meta_command(input_buffer)) {
        case META_COMMAND_SUCCESS:
          continue;
        case META_COMMAND_UNRECOGNIZED_COMMAND:
          printf("Unrecognized command '%s'\n", input_buffer->buffer);
          continue;
      }
    }

    Statement statement;

    switch (prepare_statement(input_buffer, &statement)) {
      case PREPARE_SUCCESS:
        break;
      case PREPARE_SYNTAX_ERROR:
        printf("Syntax error. Could not parse statement.\n");
        continue;
      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
        continue;
    }

    switch (execute_statement(&statement, table)) {
      case EXECUTE_SUCCESS:
        printf("Executed.\n");
        break;
      case EXECUTE_TABLE_FULL:
        printf("Error: Table full.\n");
        break;
    }
  }
}
