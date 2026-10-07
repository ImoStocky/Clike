#include "lexal.h"
#include "str.h"
#include "types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int next_char(scanner_t *s)
{
	if (s->have_unread)
	{
		s->have_unread = 0;
		return s->unread_c;
	}
	return getc(s->file);
}

static void push_char(scanner_t *s, int c)
{
	if (c == EOF)
		return;
	s->have_unread = 1;
	s->unread_c = c;
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

errv_t scanner_init(scanner_t *scanner, FILE *file)
{
	memset(scanner, 0, sizeof(*scanner));
	scanner->file = file;
	if (strInit(&scanner->buf) != 0)
		return INTER_ERR;
	scanner->active = 1;
	return COMP_OK;
}

void scanner_destroy(scanner_t *scanner)
{
	if (scanner == NULL || !scanner->active)
		return;
	strFree(&scanner->buf);
	scanner->buf.str = NULL;
	scanner->active = 0;
}

static void skip_line_comment(scanner_t *s)
{
	int c;
	for (;;)
	{
		c = next_char(s);
		if (c == EOF || c == '\n')
			return;
	}
}

static errv_t skip_block_comment(scanner_t *s)
{
	int c;
	int star = 0;
	for (;;)
	{
		c = next_char(s);
		if (c == EOF)
			return LEX_ERR;
		if (star && c == '/')
			return COMP_OK;
		star = (c == '*');
	}
}

errv_t scanner_generateToken(scanner_t *s, token_t *tok)
{
	int c;

	tok->type = END_TK;
	tok->data.lit = NULL;

	for (;;)
	{
		c = next_char(s);
		if (c == EOF)
		{
			tok->type = END_TK;
			return COMP_OK;
		}
		if (isspace(c))
			continue;
		if (c == '/')
		{
			int n = next_char(s);
			if (n == '/')
			{
				skip_line_comment(s);
				continue;
			}
			if (n == '*')
			{
				if (skip_block_comment(s) != COMP_OK)
				return LEX_ERR;
				continue;
			}
			push_char(s, n);
			tok->type = SLASH_OP;
			return COMP_OK;
		}
		break;
	}

	if (isalpha(c) || c == '_')
	{
		strClear(&s->buf);
		if (strAddChar(&s->buf, (char)c) != 0)
			return INTER_ERR;
		for (;;)
		{
			c = next_char(s);
			if (!(isalnum(c) || c == '_'))
			{
				push_char(s, c);
				break;
			}
			if (strAddChar(&s->buf, (char)c) != 0)
				return INTER_ERR;
		}
		tok->type = keyword_kind(s->buf.str);
		if (tok->type == IDENT_TK)
			tok->data.lit = xstrdup(s->buf.str);
		return COMP_OK;
	}

	if (isdigit(c))
	{
		int is_real = 0;
		strClear(&s->buf);
		if (strAddChar(&s->buf, (char)c) != 0)
			return INTER_ERR;
		for (;;)
		{
			c = next_char(s);
			if (isdigit(c))
			{
				if (strAddChar(&s->buf, (char)c) != 0)
					return INTER_ERR;
				continue;
			}
			break;
		}
		if (c == '.')
		{
			int d = next_char(s);
			if (!isdigit(d))
				return LEX_ERR;
			is_real = 1;
			if (strAddChar(&s->buf, '.') != 0 || strAddChar(&s->buf, (char)d) != 0)
				return INTER_ERR;
			for (;;)
			{
				c = next_char(s);
				if (!isdigit(c))
					break;
				if (strAddChar(&s->buf, (char)c) != 0)
					return INTER_ERR;
			}
		}
		if (c == 'e' || c == 'E')
		{
			int d;
			is_real = 1;
			if (strAddChar(&s->buf, (char)c) != 0)
				return INTER_ERR;
			d = next_char(s);
			if (d == '+' || d == '-')
			{
				if (strAddChar(&s->buf, (char)d) != 0)
					return INTER_ERR;
				d = next_char(s);
			}
			if (!isdigit(d))
				return LEX_ERR;
			if (strAddChar(&s->buf, (char)d) != 0)
				return INTER_ERR;
			for (;;)
			{
				c = next_char(s);
				if (!isdigit(c))
					break;
				if (strAddChar(&s->buf, (char)c) != 0)
					return INTER_ERR;
			}
			push_char(s, c);
		}
		else
			push_char(s, c);

		if (is_real)
		{
			tok->type = REAL_TK;
			tok->data.real = atof(s->buf.str);
		}
		else
		{
			tok->type = INT_TK;
			tok->data.ord = atoi(s->buf.str);
		}
		return COMP_OK;
	}

	if (c == '"')
	{
		strClear(&s->buf);
		for (;;)
		{
			c = next_char(s);
			if (c == EOF || c == '\n')
				return LEX_ERR;
			if (c == '"')
				break;
			if (c == '\\')
			{
				int e = next_char(s);
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
					int h1 = next_char(s);
					int h2 = next_char(s);
					int v1 = hex_val(h1);
					int v2 = hex_val(h2);
					if (v1 < 0 || v2 < 0)
						return LEX_ERR;
					c = (v1 << 4) | v2;
					if (c == 0)
						return LEX_ERR;
				}
				else
					return LEX_ERR;
			}
			else if (c < 32)
				return LEX_ERR;
			if (strAddChar(&s->buf, (char)c) != 0)
				return INTER_ERR;
		}
		tok->type = LITERAL_TK;
		tok->data.lit = xstrdup(s->buf.str);
		return COMP_OK;
	}

	switch (c)
	{
	case '+': tok->type = PLUS_OP; return COMP_OK;
	case '-': tok->type = MINUS_OP; return COMP_OK;
	case '*': tok->type = ASTERISK_OP; return COMP_OK;
	case '%': tok->type = PERCENT_OP; return COMP_OK;
	case ';': tok->type = SEMI_OP; return COMP_OK;
	case ',': tok->type = COM_OP; return COMP_OK;
	case '(': tok->type = BRACEL_OP; return COMP_OK;
	case ')': tok->type = BRACER_OP; return COMP_OK;
	case '{': tok->type = BLOCKL_OP; return COMP_OK;
	case '}': tok->type = BLOCKR_OP; return COMP_OK;
	case '!':
	{
		int n = next_char(s);
		if (n == '=')
			tok->type = NEQ_OP;
		else
		{
			push_char(s, n);
			tok->type = NOT_OP;
		}
		return COMP_OK;
	}
	case '=':
	{
		int n = next_char(s);
		if (n == '=')
			tok->type = EQ_OP;
		else
		{
			push_char(s, n);
			tok->type = ASSIGN_OP;
		}
		return COMP_OK;
	}
	case '<':
	{
		int n = next_char(s);
		if (n == '<')
			tok->type = DBL_LESS_OP;
		else if (n == '=')
			tok->type = LEE_OP;
		else
		{
			push_char(s, n);
			tok->type = LESS_OP;
		}
		return COMP_OK;
	}
	case '>':
	{
		int n = next_char(s);
		if (n == '>')
			tok->type = DBL_GRE_OP;
		else if (n == '=')
			tok->type = GRE_OP;
		else
		{
			push_char(s, n);
			tok->type = GREAT_OP;
		}
		return COMP_OK;
	}
	case '&':
	{
		int n = next_char(s);
		if (n != '&')
			return LEX_ERR;
		tok->type = AND_OP;
		return COMP_OK;
	}
	case '|':
	{
		int n = next_char(s);
		if (n != '|')
			return LEX_ERR;
		tok->type = OR_OP;
		return COMP_OK;
	}
	default:
		return LEX_ERR;
	}
	return LEX_ERR;
}
