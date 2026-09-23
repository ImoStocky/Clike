#include "lexal.h"
#include "str.h"
#include "types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *sourceFile;
static string buf;
static int have_unread;
static int unread_c;

static int next_char(void)
{
	if (have_unread)
	{
		have_unread = 0;
		return unread_c;
	}
	return getc(sourceFile);
}

static void push_char(int c)
{
	if (c == EOF)
		return;
	have_unread = 1;
	unread_c = c;
}

static int hex_val(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static token_kind_t keyword_kind(const char *s)
{
	if (strcmp(s, "auto") == 0) return AUTO_KW;
	if (strcmp(s, "cin") == 0) return CIN_KW;
	if (strcmp(s, "cout") == 0) return COUT_KW;
	if (strcmp(s, "double") == 0) return DOUBLE_KW;
	if (strcmp(s, "else") == 0) return ELSE_KW;
	if (strcmp(s, "for") == 0) return FOR_KW;
	if (strcmp(s, "if") == 0) return IF_KW;
	if (strcmp(s, "int") == 0) return INT_KW;
	if (strcmp(s, "return") == 0) return RETURN_KW;
	if (strcmp(s, "string") == 0) return STRING_KW;
	if (strcmp(s, "while") == 0) return WHILE_KW;
	if (strcmp(s, "do") == 0) return DO_KW;
	if (strcmp(s, "try") == 0) return TRY_KW;
	if (strcmp(s, "catch") == 0) return CATCH_KW;
	if (strcmp(s, "throw") == 0) return THROW_KW;
	return IDENT_TK;
}

void scanner_init(FILE *file)
{
	sourceFile = file;
	have_unread = 0;
	if (strInit(&buf) != 0)
		die(INTER_ERR);
}

static void skip_line_comment(void)
{
	int c;
	for (;;)
	{
		c = next_char();
		if (c == EOF || c == '\n')
			return;
	}
}

static void skip_block_comment(void)
{
	int c;
	int star = 0;
	for (;;)
	{
		c = next_char();
		if (c == EOF)
			die(LEX_ERR);
		if (star && c == '/')
			return;
		star = (c == '*');
	}
}

int scanner_generateToken(token_t *tok)
{
	int c;

	tok->type = END_TK;
	tok->data.lit = NULL;

	for (;;)
	{
		c = next_char();
		if (c == EOF)
		{
			tok->type = END_TK;
			return 0;
		}
		if (isspace(c))
			continue;
		if (c == '/')
		{
			int n = next_char();
			if (n == '/')
			{
				skip_line_comment();
				continue;
			}
			if (n == '*')
			{
				skip_block_comment();
				continue;
			}
			push_char(n);
			tok->type = SLASH_OP;
			return 0;
		}
		break;
	}

	if (isalpha(c) || c == '_')
	{
		strClear(&buf);
		if (strAddChar(&buf, (char)c) != 0)
			die(INTER_ERR);
		for (;;)
		{
			c = next_char();
			if (!(isalnum(c) || c == '_'))
			{
				push_char(c);
				break;
			}
			if (strAddChar(&buf, (char)c) != 0)
				die(INTER_ERR);
		}
		tok->type = keyword_kind(buf.str);
		if (tok->type == IDENT_TK)
			tok->data.lit = xstrdup(buf.str);
		return 0;
	}

	if (isdigit(c))
	{
		int is_real = 0;
		strClear(&buf);
		if (strAddChar(&buf, (char)c) != 0)
			die(INTER_ERR);
		for (;;)
		{
			c = next_char();
			if (isdigit(c))
			{
				if (strAddChar(&buf, (char)c) != 0)
					die(INTER_ERR);
				continue;
			}
			break;
		}
		if (c == '.')
		{
			int d = next_char();
			if (!isdigit(d))
				die(LEX_ERR);
			is_real = 1;
			if (strAddChar(&buf, '.') != 0 || strAddChar(&buf, (char)d) != 0)
				die(INTER_ERR);
			for (;;)
			{
				c = next_char();
				if (!isdigit(c))
					break;
				if (strAddChar(&buf, (char)c) != 0)
					die(INTER_ERR);
			}
		}
		if (c == 'e' || c == 'E')
		{
			int d;
			is_real = 1;
			if (strAddChar(&buf, (char)c) != 0)
				die(INTER_ERR);
			d = next_char();
			if (d == '+' || d == '-')
			{
				if (strAddChar(&buf, (char)d) != 0)
					die(INTER_ERR);
				d = next_char();
			}
			if (!isdigit(d))
				die(LEX_ERR);
			if (strAddChar(&buf, (char)d) != 0)
				die(INTER_ERR);
			for (;;)
			{
				c = next_char();
				if (!isdigit(c))
					break;
				if (strAddChar(&buf, (char)c) != 0)
					die(INTER_ERR);
			}
			push_char(c);
		}
		else
			push_char(c);

		if (is_real)
		{
			tok->type = REAL_TK;
			tok->data.real = atof(buf.str);
		}
		else
		{
			tok->type = INT_TK;
			tok->data.ord = atoi(buf.str);
		}
		return 0;
	}

	if (c == '"')
	{
		strClear(&buf);
		for (;;)
		{
			c = next_char();
			if (c == EOF || c == '\n')
				die(LEX_ERR);
			if (c == '"')
				break;
			if (c == '\\')
			{
				int e = next_char();
				if (e == 'n')
					c = '\n';
				else if (e == 't')
					c = '\t';
				else if (e == '\\')
					c = '\\';
				else if (e == '"')
					c = '"';
				else if (e == 'x')
				{
					int h1 = next_char();
					int h2 = next_char();
					int v1 = hex_val(h1);
					int v2 = hex_val(h2);
					if (v1 < 0 || v2 < 0)
						die(LEX_ERR);
					c = (v1 << 4) | v2;
					if (c == 0)
						die(LEX_ERR);
				}
				else
					die(LEX_ERR);
			}
			else if (c < 32)
				die(LEX_ERR);
			if (strAddChar(&buf, (char)c) != 0)
				die(INTER_ERR);
		}
		tok->type = LITERAL_TK;
		tok->data.lit = xstrdup(buf.str);
		return 0;
	}

	switch (c)
	{
	case '+': tok->type = PLUS_OP; return 0;
	case '-': tok->type = MINUS_OP; return 0;
	case '*': tok->type = ASTERISK_OP; return 0;
	case '%': tok->type = PERCENT_OP; return 0;
	case ';': tok->type = SEMI_OP; return 0;
	case ',': tok->type = COM_OP; return 0;
	case '(': tok->type = BRACEL_OP; return 0;
	case ')': tok->type = BRACER_OP; return 0;
	case '{': tok->type = BLOCKL_OP; return 0;
	case '}': tok->type = BLOCKR_OP; return 0;
	case '!':
	{
		int n = next_char();
		if (n == '=')
			tok->type = NEQ_OP;
		else
		{
			push_char(n);
			tok->type = NOT_OP;
		}
		return 0;
	}
	case '=':
	{
		int n = next_char();
		if (n == '=')
			tok->type = EQ_OP;
		else
		{
			push_char(n);
			tok->type = ASSIGN_OP;
		}
		return 0;
	}
	case '<':
	{
		int n = next_char();
		if (n == '<')
			tok->type = DBL_LESS_OP;
		else if (n == '=')
			tok->type = LEE_OP;
		else
		{
			push_char(n);
			tok->type = LESS_OP;
		}
		return 0;
	}
	case '>':
	{
		int n = next_char();
		if (n == '>')
			tok->type = DBL_GRE_OP;
		else if (n == '=')
			tok->type = GRE_OP;
		else
		{
			push_char(n);
			tok->type = GREAT_OP;
		}
		return 0;
	}
	case '&':
	{
		int n = next_char();
		if (n != '&')
			die(LEX_ERR);
		tok->type = AND_OP;
		return 0;
	}
	case '|':
	{
		int n = next_char();
		if (n != '|')
			die(LEX_ERR);
		tok->type = OR_OP;
		return 0;
	}
	default:
		die(LEX_ERR);
	}
	return 1;
}
