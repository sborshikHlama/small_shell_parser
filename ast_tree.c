#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum e_node_type {
  COMMAND,
  ARG,
  PIPE,
  REDIR_IN,
  REDIR_OUT,
  REDIR_APPEND,
  REDIR_HERE_DOC,
  END_OF_TOKENS,
}            node_type;

typedef enum e_quote_type {
  SINGLE_QUOTE,
  DOUBLE_QUOTE,
}           t_quote_type;

typedef struct s_token {
  node_type type;
  char *value;
  t_quote_type quote_type;
}              t_token;

typedef struct s_ast_node {
  node_type type;
  char  *value;
  char  **args;
  int   arg_count;
  struct s_ast_node *left;
  struct s_ast_node *right;
  char *filename;
}  t_ast_node;

#define MAX_TOKENS 100

t_ast_node *create_node(node_type type, char *value)
{
  t_ast_node *node = malloc(sizeof(t_ast_node));
  if (node == NULL)
  {
    printf("Malloc failed");
    return (NULL);
  }
  node->type = type;
  node->value = value ? strdup(value) : NULL;
  node->args = NULL;
  node->left = NULL;
  node->right = NULL;
  node->filename = NULL;
  node->arg_count = 0;
  return (node);
}

node_type get_token_type(char *token)
{
  if (strcmp(token, "|") == 0)
    return (PIPE);
  else if (strcmp(token, "<") == 0)
    return (REDIR_IN);
  else if (strcmp(token, ">") == 0)
    return (REDIR_OUT);
  else if (strcmp(token, ">>") == 0)
    return (REDIR_APPEND);
  else if (strcmp(token, "<<") == 0)
    return (REDIR_HERE_DOC);
  else
    return (COMMAND);
}

char *extract_quoted_token(char *input, int *pos)
{
  char  quote_char;
  int   start;
  int   len;
  char  *token;

  quote_char = input[*pos];
  start = *pos;
  (*pos)++;
  len = 0;
  while(input[*pos] && input[*pos] != quote_char)
  {
    (*pos)++;
    len++;
  }
  if (input[*pos] == quote_char)
  {
    len++;
    token = strndup(&input[start], len + 1);
    (*pos)++;
    return (token);
  }
  return (NULL);
}

int is_quote(char c)
{
  return (c == '\'' || c == '"');
}

int is_space(char c)
{
  return (c == ' ' || (c >= 9 && c <= 13));
}

int is_operator(char *token)
{
  if (!token)
    return (0);
  return (strcmp(token, "|") == 0 ||
          strcmp(token, "<") == 0 ||
          strcmp(token, ">") == 0 ||
          strcmp(token, "<<") == 0 ||
          strcmp(token, ">>") == 0);
}

char  **tokenize_input(char *input)
{
  char  **tokens;
  int   i;
  int   token_index;
  int   start;

  tokens = malloc(sizeof(char *) * MAX_TOKENS);
  if (input == NULL || tokens == NULL)
  {
    printf("Command is null");
    return (NULL);
  }
  i = 0;
  token_index = 0;
  while (input[i])
  {
    while (input[i] && input[i] == ' ')
      i++;
    if (!input[i])
      break;
    if (is_quote(input[i]))
    {
        tokens[token_index] = extract_quoted_token(input, &i);
        if (!tokens[token_index])
        {
          printf("Error: couldn't extract quotes");
        }
    }
    else
    {
      start = i;
      while (input[i] && !is_quote(input[i]) && !is_space(input[i]))
        i++;
      tokens[token_index] = strndup(&input[start], i - start);
      if (!tokens[token_index])
      {
        printf("Error: coudn't allocate token");
      }
    }
    token_index++;
  }
  tokens[token_index] = NULL;
  return (tokens);
}

size_t  count_tokens(char **tokens)
{
  size_t  len;
  len = 0;

  while(tokens[len] != NULL)
    len++;
  return (len);
}

t_token  **lexer(char *input)
{
  char  **raw_tokens;
  t_token **tokens;
  size_t  tokens_len;
  int     i;
  int     is_first_token;

  raw_tokens = tokenize_input(input);
  if (!raw_tokens)
  {
    printf("Error: couldn't tokenize input");
    return (NULL);
  }
  tokens_len = count_tokens(raw_tokens);
  tokens = malloc(sizeof(t_token *) * (tokens_len + 1));
  if (tokens == NULL)
  {
    printf("Error: malloc tokens failed");
    return (NULL);
  }
  i = 0;
  is_first_token = 1;
  while (raw_tokens[i])
  {
    tokens[i] = malloc(sizeof(t_token));
    if (is_first_token)
    {
      tokens[i]->type = get_token_type(raw_tokens[i]);
      is_first_token = 0;
    }
    else if (!is_operator(raw_tokens[i]))
    {
      tokens[i]->type = ARG;
    }
    else
    {
      tokens[i]->type = get_token_type(raw_tokens[i]);
      is_first_token = 1;
    }
    tokens[i]->value = raw_tokens[i];
    i++;
  }
  tokens[i] = NULL;
  return (tokens);
}

/*
BNF
sequence    → pipeline (";" pipeline)*
pipeline    → command ("|" command)*
command     → WORD ARG*
arg         → WORD
*/

t_ast_node *parse_command(t_token **tokens, int *pos)
{
  t_ast_node  *command_node;
  t_ast_node  *current_ptr;
  int         args_count;
  int         i;
  if (!tokens[*pos] || tokens[*pos]->type != COMMAND)
    return (NULL);
  command_node = create_node(COMMAND, tokens[*pos]->value);
  if (!command_node)
    return (NULL);
  (*pos)++;
  i = *pos;
  args_count = 0;
  while (tokens[i] && tokens[i]->type == ARG)
  {
    args_count++;
    i++;
  }
  command_node->arg_count = args_count;
  command_node->args = malloc(sizeof(char) * args_count + 1);
  if (!command_node->args)
    command_node->args = NULL;
  i = 0;
  while (tokens[*pos] && tokens[*pos]->type == ARG)
  {
     command_node->args[i] = strdup(tokens[*pos]->value);
     i++;
     (*pos)++; 
  }
  //command_node->args[args_count] = NULL;
  return (command_node);
}

t_ast_node *parser(t_token **tokens, int *pos)
{
  t_ast_node *left;
  t_ast_node *root_node;

  if (!tokens || !tokens[*pos])
      return NULL;
  left = NULL;
  root_node = NULL;
  if (tokens == NULL)
  {
    printf("Error: tokens array is NULL");
    return (NULL);
  }
  /*First token should always be type Word*/
  left = parse_command(tokens, pos);
  if (left == NULL)
    return (NULL);
  if (!tokens[*pos] || tokens[*pos]->type != PIPE)
    return left;
  root_node = create_node(tokens[*pos]->type, tokens[*pos]->value);
  if (!root_node)
    return NULL;
  (*pos)++;
  root_node->left = left;
  root_node->right = parser(tokens, pos); 
  return (root_node);
}

void  printf_tokens_debug(t_token **tokens)
{
  int i = 0;
  while (tokens[i] != NULL)
  {
    printf("Value: %s ", tokens[i]->value);
    if (tokens[i]->type == COMMAND)
      printf("Type: COMMAND\n");
    else if (tokens[i]->type == ARG)
      printf("Type: ARG\n");
    else if (tokens[i]->type == PIPE)
      printf("Type: PIPE\n");
    else if (tokens[i]->type == REDIR_APPEND)
      printf("Type: REDID_APPEND\n");
    else if (tokens[i]->type == REDIR_HERE_DOC)
      printf("Type: REDIR_HERE_DOC\n");
    else if (tokens[i]->type == REDIR_IN)
      printf("Type: REDIR_IN\n");
    else if (tokens[i]->type == REDIR_OUT)
      printf("Type: REDIR_OUT\n");
    else
      printf("UNNOWN TYPE\n");
    i++;
  }
}

// void  print_ast(t_ast_node *node, int depth)
// {
//   if (!node)
// }

void print_ast(t_ast_node *node, int depth)
{
    if (!node)
        return;

    // Print indentation
    for (int i = 0; i < depth; i++)
        printf("  ");

    // Print node type and value
    printf("└─ ");
    if (node->type == COMMAND)
    {
        printf("CMD: %s\n", node->value);
        // Print arguments if any
        if (node->args)
        {
            for (int i = 0; i < node->arg_count; i++)
            {
                for (int j = 0; j < depth + 1; j++)
                    printf("  ");
                printf("└─ ARG: %s\n", node->args[i]);
            }
        }
    }
    else if (node->type == PIPE)
        printf("PIPE\n");
    else if (node->type == REDIR_IN)
        printf("REDIR_IN\n");
    else if (node->type == REDIR_OUT)
        printf("REDIR_OUT\n");
    else if (node->type == REDIR_APPEND)
        printf("REDIR_APPEND\n");
    else if (node->type == REDIR_HERE_DOC)
        printf("REDIR_HERE_DOC\n");
    else
        printf("UNKNOWN\n");

    // Recursively print left and right subtrees
    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
}

int main(int argc, char **argv, char **envp)
{
  char command[] = "cat file.txt | grep hello | wc -l";
  t_token **lexical_string = lexer(command);
  int pos = 0;
  printf_tokens_debug(lexical_string);
  t_ast_node *node = parser(lexical_string, &pos);
  print_ast(node, 0);
  // while (*parsed_command != NULL)
  // {
  //   printf("%s\n", *parsed_command);
  //   parsed_command++;
  // }
}
