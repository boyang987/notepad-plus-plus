// Scintilla source code edit control
/** @file LexCoffeeScript.cxx
 ** Lexer for CoffeeScript.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// Based on the Scintilla C++ Lexer
// Written by Eric Promislow <ericp@activestate.com> in 2011 for the Komodo IDE
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "Platform.h"
#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

static bool IsSpaceEquiv(int state) {
	return (state == SCE_COFFEESCRIPT_DEFAULT
	    || state == SCE_COFFEESCRIPT_COMMENTLINE
	    || state == SCE_COFFEESCRIPT_COMMENTBLOCK
	    || state == SCE_COFFEESCRIPT_VERBOSE_REGEX
	    || state == SCE_COFFEESCRIPT_VERBOSE_REGEX_COMMENT
	    || state == SCE_COFFEESCRIPT_WORD
	    || state == SCE_COFFEESCRIPT_REGEX);
}

// Preconditions: sc.currentPos points to a character after '+' or '-'.
// The test for pos reaching 0 should be redundant,
// and is in only for safety measures.
// Limitation: this code will give the incorrect answer for code like
// a = b+++/ptn/...
// Putting a space between the '++' post-inc operator and the '+' binary op
// fixes this, and is highly recommended for readability anyway.
static bool FollowsPostfixOperator(StyleContext &sc, Accessor &styler) {
	int pos = (int) sc.currentPos;
	while (--pos > 0) {
		char ch = styler[pos];
		if (ch == '+' || ch == '-') {
			return styler[pos - 1] == ch;
		}
	}
	return false;
}

static bool followsReturnKeyword(StyleContext &sc, Accessor &styler) {
    // Don't look at styles, so no need to flush.
	int pos = (int) sc.currentPos;
	int currentLine = styler.GetLine(pos);
	int lineStartPos = styler.LineStart(currentLine);
	while (--pos > lineStartPos) {
		char ch = styler.SafeGetCharAt(pos);
		if (ch != ' ' && ch != '\t') {
			break;
		}
	}
	const char *retBack = "nruter";
	const char *s = retBack;
	while (*s
	       && pos >= lineStartPos
	       && styler.SafeGetCharAt(pos) == *s) {
		s++;
		pos--;
	}
	return !*s;
}

static void ColouriseCoffeeScriptDoc(unsigned int startPos, int length, int initStyle, WordList *keywordlists[],
                            Accessor &styler) {

	WordList &keywords = *keywordlists[0];
	WordList &keywords2 = *keywordlists[1];
	WordList &keywords4 = *keywordlists[3];

	CharacterSet setOKBeforeRE(CharacterSet::setNone, "([{=,:;!%^&*|?~+-");
	CharacterSet setCouldBePostOp(CharacterSet::setNone, "+-");

	CharacterSet setWordStart(CharacterSet::setAlpha, "_$@", 0x80, true);
	CharacterSet setWord(CharacterSet::setAlphaNum, "._$", 0x80, true);

	int chPrevNonWhite = ' ';
	int visibleChars = 0;

	// look back to set chPrevNonWhite properly for better regex colouring
	int endPos = startPos + length;
        if (startPos > 0 && IsSpaceEquiv(initStyle)) {
		unsigned int back = startPos;
		styler.Flush();
		while (back > 0 && IsSpaceEquiv(styler.StyleAt(--back)))
			;
		if (styler.StyleAt(back) == SCE_COFFEESCRIPT_OPERATOR) {
			chPrevNonWhite = styler.SafeGetCharAt(back);
		}
		if (startPos != back) {
			initStyle = styler.StyleAt(back);
			if (IsSpaceEquiv(initStyle)) {
				initStyle = SCE_COFFEESCRIPT_DEFAULT;
			}
		}
		startPos = back;
	}

	StyleContext sc(startPos, endPos - startPos, initStyle, styler);

	for (; sc.More(); sc.Forward()) {

		if (sc.atLineStart) {
			// Reset states to beginning of colourise so no surprises
			// if different sets of lines lexed.
			visibleChars = 0;
		}

		// Determine if the current state should terminate.
		switch (sc.state) {
			case SCE_COFFEESCRIPT_OPERATOR:
				sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				break;
			case SCE_COFFEESCRIPT_NUMBER:
				// We accept almost anything because of hex. and number suffixes
				if (!setWord.Contains(sc.ch) || sc.Match('.', '.')) {
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_IDENTIFIER:
				if (!setWord.Contains(sc.ch) || (sc.ch == '.') || (sc.ch == '$')) {
					char s[1000];
					sc.GetCurrent(s, sizeof(s));
					if (keywords.InList(s)) {
						sc.ChangeState(SCE_COFFEESCRIPT_WORD);
					} else if (keywords2.InList(s)) {
						sc.ChangeState(SCE_COFFEESCRIPT_WORD2);
					} else if (keywords4.InList(s)) {
						sc.ChangeState(SCE_COFFEESCRIPT_GLOBALCLASS);
					}
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_WORD:
			case SCE_COFFEESCRIPT_WORD2:
			case SCE_COFFEESCRIPT_GLOBALCLASS:
				if (!setWord.Contains(sc.ch)) {
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_COMMENTLINE:
				if (sc.atLineStart) {
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_STRING:
				if (sc.ch == '\\') {
					if (sc.chNext == '\"' || sc.chNext == '\'' || sc.chNext == '\\') {
						sc.Forward();
					}
				} else if (sc.ch == '\"') {
					sc.ForwardSetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_CHARACTER:
				if (sc.ch == '\\') {
					if (sc.chNext == '\"' || sc.chNext == '\'' || sc.chNext == '\\') {
						sc.Forward();
					}
				} else if (sc.ch == '\'') {
					sc.ForwardSetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_REGEX:
				if (sc.atLineStart) {
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				} else if (sc.ch == '/') {
					sc.Forward();
					while ((sc.ch < 0x80) && islower(sc.ch))
						sc.Forward();    // gobble regex flags
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				} else if (sc.ch == '\\') {
					// Gobble up the quoted character
					if (sc.chNext == '\\' || sc.chNext == '/') {
						sc.Forward();
					}
				}
				break;
			case SCE_COFFEESCRIPT_STRINGEOL:
				if (sc.atLineStart) {
					sc.SetState(SCE_COFFEESCRIPT_DEFAULT);
				}
				break;
			case SCE_COFFEESCRIPT_COMMENTBLOCK:
				if (sc.Match("###")) {
					sc.Forward();
					sc.Forward();
					sc.ForwardSetState(SCE_COFFEESCRIPT_DEFAULT);
				} else if (sc.ch == '\\') {
					sc.Forward();
				}
				break;
			case SCE_COFFEESCRIPT_VERBOSE_REGEX:
				if (sc.Match("///")) {
					sc.Forward();
					sc.Forward();
					sc.ForwardSetState(SCE_COFFEESCRIPT_DEFAULT);
				} else if (sc.Match('#')) {
					sc.SetState(SCE_COFFEESCRIPT_VERBOSE_REGEX_COMMENT);
				} else if (sc.ch == '\\') {
					sc.Forward();
				}
				break;
			case SCE_COFFEESCRIPT_VERBOSE_REGEX_COMMENT:
				if (sc.atLineStart) {
					sc.SetState(SCE_COFFEESCRIPT_VERBOSE_REGEX);
				}
				break;
		}

		// Determine if a new state should be entered.
		if (sc.state == SCE_COFFEESCRIPT_DEFAULT) {
			if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_COFFEESCRIPT_NUMBER);
			} else if (setWordStart.Contains(sc.ch)) {
				sc.SetState(SCE_COFFEESCRIPT_IDENTIFIER);
			} else if (sc.Match("///")) {
				sc.SetState(SCE_COFFEESCRIPT_VERBOSE_REGEX);
				sc.Forward();
				sc.Forward();
			} else if (sc.ch == '/'
				   && (setOKBeforeRE.Contains(chPrevNonWhite)
				       || followsReturnKeyword(sc, styler))
				   && (!setCouldBePostOp.Contains(chPrevNonWhite)
				       || !FollowsPostfixOperator(sc, styler))) {
				sc.SetState(SCE_COFFEESCRIPT_REGEX);	// JavaScript's RegEx
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_COFFEESCRIPT_STRING);
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_COFFEESCRIPT_CHARACTER);
			} else if (sc.ch == '#') {
				if (sc.Match("###")) {
					sc.SetState(SCE_COFFEESCRIPT_COMMENTBLOCK);
					sc.Forward();
					sc.Forward();
				} else {
					sc.SetState(SCE_COFFEESCRIPT_COMMENTLINE);
				}
			} else if (isoperator(static_cast<char>(sc.ch))) {
				sc.SetState(SCE_COFFEESCRIPT_OPERATOR);
			}
		}

		if (!IsASpace(sc.ch) && !IsSpaceEquiv(sc.state)) {
			chPrevNonWhite = sc.ch;
			visibleChars++;
		}
	}
	sc.Complete();
}

static bool IsCommentLine(int line, Accessor &styler) {
	int pos = styler.LineStart(line);
	int eol_pos = styler.LineStart(line + 1) - 1;
	for (int i = pos; i < eol_pos; i++) {
		char ch = styler[i];
		if (ch == '#')
			return true;
		else if (ch != ' ' && ch != '\t')
			return false;
	}
	return false;
}

static void FoldCoffeeScriptDoc(unsigned int startPos, int length, int,
				WordList *[], Accessor &styler) {
	// A simplified version of FoldPyDoc
	const int maxPos = startPos + length;
	const int maxLines = styler.GetLine(maxPos - 1);             // Requested last line
	const int docLines = styler.GetLine(styler.Length() - 1);  // Available last line

	// property fold.coffeescript.comment
	const bool foldComment = styler.GetPropertyInt("fold.coffeescript.comment") != 0;

	const bool foldCompact = styler.GetPropertyInt("fold.compact") != 0;

	// Backtrack to previous non-blank line so we can determine indent level
	// for any white space lines
	// and so we can fix any preceding fold level (which is why we go back
	// at least one line in all cases)
	int spaceFlags = 0;
	int lineCurrent = styler.GetLine(startPos);
	int indentCurrent = styler.IndentAmount(lineCurrent, &spaceFlags, NULL);
	while (lineCurrent > 0) {
		lineCurrent--;
		indentCurrent = styler.IndentAmount(lineCurrent, &spaceFlags, NULL);
		if (!(indentCurrent & SC_FOLDLEVELWHITEFLAG)
		    && !IsCommentLine(lineCurrent, styler))
			break;
	}
	int indentCurrentLevel = indentCurrent & SC_FOLDLEVELNUMBERMASK;

	// Set up initial loop state
	int prevComment = 0;
	if (lineCurrent >= 1)
		prevComment = foldComment && IsCommentLine(lineCurrent - 1, styler);

	// Process all characters to end of requested range
	// or comment that hangs over the end of the range.  Cap processing in all cases
	// to end of document (in case of comment at end).
	while ((lineCurrent <= docLines) && ((lineCurrent <= maxLines) || prevComment)) {

		// Gather info
		int lev = indentCurrent;
		int lineNext = lineCurrent + 1;
		int indentNext = indentCurrent;
		if (lineNext <= docLines) {
			// Information about next line is only available if not at end of document
			indentNext = styler.IndentAmount(lineNext, &spaceFlags, NULL);
		}
		const int comment = foldComment && IsCommentLine(lineCurrent, styler);
		const int comment_start = (comment && !prevComment && (lineNext <= docLines) &&
		                           IsCommentLine(lineNext, styler) && (lev > SC_FOLDLEVELBASE));
		const int comment_continue = (comment && prevComment);
		if (!comment)
			indentCurrentLevel = indentCurrent & SC_FOLDLEVELNUMBERMASK;
		if (indentNext & SC_FOLDLEVELWHITEFLAG)
			indentNext = SC_FOLDLEVELWHITEFLAG | indentCurrentLevel;

		if (comment_start) {
			// Place fold point at start of a block of comments
			lev |= SC_FOLDLEVELHEADERFLAG;
		} else if (comment_continue) {
			// Add level to rest of lines in the block
			lev = lev + 1;
		}

		// Skip past any blank lines for next indent level info; we skip also
		// comments (all comments, not just those starting in column 0)
		// which effectively folds them into surrounding code rather
		// than screwing up folding.

		while ((lineNext < docLines) &&
		        ((indentNext & SC_FOLDLEVELWHITEFLAG) ||
		         (lineNext <= docLines && IsCommentLine(lineNext, styler)))) {

			lineNext++;
			indentNext = styler.IndentAmount(lineNext, &spaceFlags, NULL);
		}

		const int levelAfterComments = indentNext & SC_FOLDLEVELNUMBERMASK;
		const int levelBeforeComments = Platform::Maximum(indentCurrentLevel,levelAfterComments);

		// Now set all the indent levels on the lines we skipped
		// Do this from end to start.  Once we encounter one line
		// which is indented more than the line after the end of
		// the comment-block, use the level of the block before

		int skipLine = lineNext;
		int skipLevel = levelAfterComments;

		while (--skipLine > lineCurrent) {
			int skipLineIndent = styler.IndentAmount(skipLine, &spaceFlags, NULL);

			if (foldCompact) {
				if ((skipLineIndent & SC_FOLDLEVELNUMBERMASK) > levelAfterComments)
					skipLevel = levelBeforeComments;

				int whiteFlag = skipLineIndent & SC_FOLDLEVELWHITEFLAG;

				styler.SetLevel(skipLine, skipLevel | whiteFlag);
			} else {
				if ((skipLineIndent & SC_FOLDLEVELNUMBERMASK) > levelAfterComments &&
					!(skipLineIndent & SC_FOLDLEVELWHITEFLAG) &&
					!IsCommentLine(skipLine, styler))
					skipLevel = levelBeforeComments;

				styler.SetLevel(skipLine, skipLevel);
			}
		}

		// Set fold header on non-comment line
		if (!comment && !(indentCurrent & SC_FOLDLEVELWHITEFLAG)) {
			if ((indentCurrent & SC_FOLDLEVELNUMBERMASK) < (indentNext & SC_FOLDLEVELNUMBERMASK))
				lev |= SC_FOLDLEVELHEADERFLAG;
		}

		// Keep track of block comment state of previous line
		prevComment = comment_start || comment_continue;

		// Set fold level for this line and move to next line
		styler.SetLevel(lineCurrent, lev);
		indentCurrent = indentNext;
		lineCurrent = lineNext;
	}
}

static const char *const csWordLists[] = {
            "Keywords",
            "Secondary keywords",
            "Unused",
            "Global classes",
            0,
};

LexerModule lmCoffeeScript(SCLEX_COFFEESCRIPT, ColouriseCoffeeScriptDoc, "coffeescript", FoldCoffeeScriptDoc, csWordLists);
