#include "U_edit.h"
#include "../InputFocusManager.h"
#include "../header/UISafeText.h"
#include <algorithm>

// ============================================================================
// UTF-8 <-> UTF-32 Conversion (from textbox_demo.c)
// ============================================================================

static int utf32_to_utf8(uint32_t c, char out[5]) {
	if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
		c = 0xFFFD;
	if (c < 0x80) {
		out[0] = c;
		out[1] = 0;
		return 1;
	} else if (c < 0x800) {
		out[0] = 0xC0 | (c >> 6);
		out[1] = 0x80 | (c & 0x3F);
		out[2] = 0;
		return 2;
	} else if (c < 0x10000) {
		out[0] = 0xE0 | (c >> 12);
		out[1] = 0x80 | ((c >> 6) & 0x3F);
		out[2] = 0x80 | (c & 0x3F);
		out[3] = 0;
		return 3;
	} else {
		out[0] = 0xF0 | (c >> 18);
		out[1] = 0x80 | ((c >> 12) & 0x3F);
		out[2] = 0x80 | ((c >> 6) & 0x3F);
		out[3] = 0x80 | (c & 0x3F);
		out[4] = 0;
		return 4;
	}
}

// ============================================================================
// LineEditor implementation
// ============================================================================

LineEditor::LineEditor()
{
	memset(m_Text, 0, sizeof(m_Text));
	m_CursorPos = 0;
	m_TextLen = 0;
	m_Limit = MAX_TEXT - 1;
	m_bAcquired = false;
	memset(m_Composing, 0, sizeof(m_Composing));
	m_ComposingLen = 0;
}

void LineEditor::Acquire()
{
	m_bAcquired = true;
}

void LineEditor::Unacquire()
{
	m_bAcquired = false;
	EndComposition();
}

bool LineEditor::IsAcquire() const
{
	return m_bAcquired;
}

// Insert UTF-32 text at cursor position
void LineEditor::InsertText(const uint32_t* text, int len)
{
	const int limit = std::clamp(m_Limit, 0, MAX_TEXT - 1);
	if (text == nullptr || len <= 0 || m_TextLen < 0 || m_TextLen > limit ||
		m_CursorPos < 0 || m_CursorPos > m_TextLen || len > limit - m_TextLen)
		return;

	// Snapshot before shifting: callers may insert a slice of m_Text itself.
	uint32_t insertion[MAX_TEXT];
	memcpy(insertion, text, static_cast<size_t>(len) * sizeof(uint32_t));

	// Move existing text to make room
	memmove(&m_Text[m_CursorPos + len],
	        &m_Text[m_CursorPos],
	        (m_TextLen - m_CursorPos) * sizeof(uint32_t));

	// Insert new text
	memcpy(&m_Text[m_CursorPos], insertion, static_cast<size_t>(len) * sizeof(uint32_t));

	m_CursorPos += len;
	m_TextLen += len;
}

// Insert single UTF-32 character
void LineEditor::InsertChar(uint32_t c)
{
	InsertText(&c, 1);
}

// Delete character at offset
void LineEditor::DeleteChar(int offset)
{
	if (offset < 0 || offset >= m_TextLen) return;

	memmove(&m_Text[offset],
	        &m_Text[offset + 1],
	        (m_TextLen - offset - 1) * sizeof(uint32_t));

	m_TextLen--;

	// Adjust cursor position if it was after the deleted character
	if (m_CursorPos > offset) {
		m_CursorPos--;
	}
	// Also ensure cursor doesn't go beyond text length
	if (m_CursorPos > m_TextLen) {
		m_CursorPos = m_TextLen;
	}
}

// Delete character before cursor (backspace)
void LineEditor::Backspace()
{
	if (m_CursorPos > 0) {
		// DeleteChar will handle cursor adjustment
		DeleteChar(m_CursorPos - 1);
	}
}

// Move cursor by delta characters
void LineEditor::MoveCursor(int delta)
{
	const int64_t newPos = static_cast<int64_t>(m_CursorPos) + delta;
	m_CursorPos = static_cast<int>(std::clamp<int64_t>(newPos, 0, m_TextLen));
}

// Set cursor to absolute position
void LineEditor::SetCursor(int pos)
{
	if (pos < 0) pos = 0;
	if (pos > m_TextLen) pos = m_TextLen;
	m_CursorPos = pos;
}

// Handle SDL_TEXTINPUT event (committed text)
void LineEditor::HandleTextInput(const char* text)
{
	if (text == NULL || text[0] == '\0') return;

	uint32_t utf32[32];
	int len = (int)UISafeText::Utf8ToUtf32(text, utf32, 32);

	if (IsComposing()) {
		EndComposition();
	}

	InsertText(utf32, len);
}

// Handle SDL_TEXTEDITING event (IME composition in progress)
void LineEditor::HandleTextEditing(const char* text, int /*start*/, int /*length*/)
{
	// SDL's start/length describe the selection, which can be empty while
	// preedit text still exists. Only SDL_TEXTINPUT commits that text.
	if (text != nullptr && text[0] != '\0') {
		m_ComposingLen = (int)UISafeText::Utf8ToUtf32(text, m_Composing, MAX_TEXT);
	} else {
		// Composition ended
		EndComposition();
	}
}

// Start IME composition
void LineEditor::StartComposition(const char* text, int start, int length)
{
	HandleTextEditing(text, start, length);
}

// Update IME composition
void LineEditor::UpdateComposition(const char* text, int start, int length)
{
	HandleTextEditing(text, start, length);
}

// Discard preedit state. Committed text arrives separately in HandleTextInput.
void LineEditor::EndComposition()
{
	m_ComposingLen = 0;
}

// Get text as UTF-8 string (for compatibility)
std::string LineEditor::GetBuffer() const
{
	std::string result;

	for (int i = 0; i < m_TextLen && i < MAX_TEXT; ++i) {
		char buf[5];
		int len = utf32_to_utf8(m_Text[i], buf);
		result.append(buf, len);
	}
	return result;
}

const char* LineEditor::GetString() const
{
	m_LegacyText = GetBuffer();
	return m_LegacyText.c_str();
}

// Legacy: Add UTF-8 string (converts to UTF-32 internally)
void LineEditor::AddString(const char* pStr)
{
	if (pStr == NULL) return;

	uint32_t utf32[MAX_TEXT];
	int len = (int)UISafeText::Utf8ToUtf32(pStr, utf32, MAX_TEXT);

	InsertText(utf32, len);
}

void LineEditor::SetByteLimit(int limit)
{
	m_Limit = std::clamp(limit, 0, MAX_TEXT - 1);
}

// Legacy: Clear all text
void LineEditor::EraseAll()
{
	m_TextLen = 0;
	m_CursorPos = 0;
	memset(m_Text, 0, sizeof(m_Text));
	m_ComposingLen = 0;
}

// Legacy: Delete character before cursor
void LineEditor::EraseCharacterBegin()
{
	Backspace();
}

// Legacy: Insert special mark
void LineEditor::InsertMark(unsigned short mark)
{
	InsertChar((uint32_t)mark);
}

// KeyboardControl - main entry point for keyboard messages
void LineEditor::KeyboardControl(unsigned int message, unsigned int key, long extra)
{
	(void)extra;
	switch (message)
	{
	case WM_CHAR:
		// Legacy: Single character input
		InsertChar((uint32_t)key);
		break;

	// No WM_TEXTINPUT case: committed text reaches HandleTextInput() from the
	// SDL backend by way of InputFocusManager, as a const char*. It must not
	// arrive here, because `extra` is a `long` - 32-bit on LLP64 - and would
	// truncate the pointer.

	case WM_TEXTEDITING:
		// SDL_TEXTEDITING event (composition)
		// Note: This is a simplified handling - actual implementation would need the text
		// For now, we just clear composition state when we get this message
		m_ComposingLen = 0;
		break;

	case WM_KEYDOWN:
		// Control keys
		switch (key)
		{
		case VK_BACK:
			Backspace();
			break;
		case VK_LEFT:
			MoveCursor(-1);
			break;
		case VK_RIGHT:
			MoveCursor(1);
			break;
		case VK_HOME:
			SetCursor(0);
			break;
		case VK_END:
			SetCursor(m_TextLen);
			break;
		case VK_DELETE:
			DeleteChar(m_CursorPos);
			break;
		}
		break;
	}
}

// ============================================================================
// LineEditorVisual implementation
// ============================================================================

LineEditorVisual::LineEditorVisual()
{
	// Debug: print offset
//	printf("DEBUG LineEditorVisual::LineEditorVisual: this=%p, &m_Editor=%p, &m_Editor.m_CursorPos=%p\n",
//	       this, &m_Editor, &m_Editor.m_CursorPos);

	m_X = 0;
	m_Y = 0;
	m_AbsWidth = 100;
	m_bPasswordMode = false;
	m_bAcquired = false;
	m_PrintInfo.hfont = NULL;
	m_PrintInfo.text_color = 0xFFFFFF;
	m_PrintInfo.back_color = 0;
	m_PrintInfo.bk_mode = 0;
	m_PrintInfo.text_align = 0;
	m_CursorColor = 0xFFFFFF;
}

LineEditorVisual::~LineEditorVisual()
{
	// Clear focus from InputFocusManager if this editor was focused
	if (InputFocusManager::GetInstance().GetFocusedEditor() == this) {
		InputFocusManager::GetInstance().SetFocusedEditor(NULL);
	}
}

void LineEditorVisual::Acquire()
{
	// The focus owner controls acquisition state and SDL text input together.
	InputFocusManager::GetInstance().SetFocusedEditor(this);
}

void LineEditorVisual::Unacquire()
{
	// Release focus if this editor was focused
	if (InputFocusManager::GetInstance().GetFocusedEditor() == this) {
		InputFocusManager::GetInstance().SetFocusedEditor(NULL);
	}

	m_Editor.Unacquire();
	m_bAcquired = false;
}

void LineEditorVisual::SetPosition(int x, int y)
{
	m_X = x;
	m_Y = y;
}

void LineEditorVisual::SetAbsWidth(int width)
{
	m_AbsWidth = width > 0 ? width : 0;
}

void LineEditorVisual::SetPrintInfo(PrintInfo& info)
{
	m_PrintInfo = info;
}

void LineEditorVisual::SetCursorColor(unsigned long color)
{
	m_CursorColor = color;
}

void LineEditorVisual::PasswordMode(bool bPassword)
{
	m_bPasswordMode = bPassword;
}

int LineEditorVisual::GetLineCount() const
{
	return 1;
}

bool LineEditorVisual::ReachSizeOfBox() const
{
	// Use a default font size since PrintInfo doesn't have a size field
	const int DEFAULT_FONT_SIZE = 12;
	int len = m_Editor.GetTextLen();
	return (len * DEFAULT_FONT_SIZE) >= m_AbsWidth;
}

// Compatibility method: convert UTF-32 to wide string (char_t/UTF-16LE)
std::basic_string<char_t> LineEditorVisual::GetStringWide() const
{
	std::basic_string<char_t> result;

	// Convert directly from UTF-32 (m_Text) to UTF-16 (char_t)
	for (int i = 0; i < m_Editor.m_TextLen && i < LineEditor::MAX_TEXT; ++i) {
		uint32_t c = m_Editor.m_Text[i];
		if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
			c = 0xFFFD;

		// UTF-32 to UTF-16 conversion
		if (c < 0x10000) {
			// BMP character - single UTF-16 code unit
			result.push_back(static_cast<char_t>(c));
		} else {
			// Supplementary plane - surrogate pair
			c -= 0x10000;
			result.push_back(static_cast<char_t>(0xD800 + (c >> 10)));
			result.push_back(static_cast<char_t>(0xDC00 + (c & 0x3FF)));
		}
	}
	return result;
}

Point LineEditorVisual::GetPosition() const
{
	Point p;
	p.Set(m_X, m_Y);
	return p;
}
