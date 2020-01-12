#include "Keyboard.h"
#include "SpriteManager.h"
#include "StringUtils.h"

Keyboard::Keyboard(SpriteManager& spr_mngr) : sprMngr(spr_mngr)
{
#define MAKE_KEY_CODE(name, index, code) \
    KeysMap[code] = index; \
    KeysMapRevert[index] = code
#include "KeyCodes_Include.h"

    // User keys mapping
    for (uint i = 0; i < 0x100; i++)
        KeysMapUser[i] = i;

    istringstream str(GameOpt.KeyboardRemap);
    while (!str.eof())
    {
        int from, to;
        str >> from >> to;
        if (str.fail())
            break;
        from &= 0xFF;
        to &= 0xFF;
        KeysMapUser[from] = to;
    }
}

Keyboard::~Keyboard()
{
}

void Keyboard::Lost()
{
    CtrlDwn = false;
    AltDwn = false;
    ShiftDwn = false;
}

void Keyboard::GetChar(uchar dik, const string& dik_text, string& str, uint* position, uint max, int flags)
{
    if (AltDwn)
        return;

    bool ctrl_shift = (CtrlDwn || ShiftDwn);

    uint dik_text_len_utf8 = _str(dik_text).lengthUtf8();
    uint str_len_utf8 = _str(str).lengthUtf8();
    uint str_len = (uint)str.length();

    uint position_dummy = str_len;
    uint& pos = (position ? *position : position_dummy);
    if (pos > str_len)
        pos = str_len;

    // Controls
    if (dik == DIK_RIGHT && !ctrl_shift)
    {
        if (pos < str_len)
        {
            pos++;
            while (pos < str_len && (str[pos] & 0xC0) == 0x80)
                pos++;
        }
    }
    else if (dik == DIK_LEFT && !ctrl_shift)
    {
        if (pos > 0)
        {
            pos--;
            while (pos && (str[pos] & 0xC0) == 0x80)
                pos--;
        }
    }
    else if (dik == DIK_BACK && !ctrl_shift)
    {
        if (pos > 0)
        {
            uint letter_len = 1;
            pos--;
            while (pos && (str[pos] & 0xC0) == 0x80)
                pos--, letter_len++;

            str.erase(pos, letter_len);
        }
    }
    else if (dik == DIK_DELETE && !ctrl_shift)
    {
        if (pos < str_len)
        {
            uint letter_len = 1;
            uint pos_ = pos + 1;
            while (pos_ < str_len && (str[pos_] & 0xC0) == 0x80)
                pos_++, letter_len++;

            str.erase(pos, letter_len);
        }
    }
    else if (dik == DIK_HOME && !ctrl_shift)
    {
        pos = 0;
    }
    else if (dik == DIK_END && !ctrl_shift)
    {
        pos = str_len;
    }
    // Clipboard
    else if (CtrlDwn && !ShiftDwn && str_len > 0 && (dik == DIK_C || dik == DIK_X))
    {
        SDL_SetClipboardText(str.c_str());
        if (dik == DIK_X)
        {
            str.clear();
            pos = 0;
        }
    }
    else if (CtrlDwn && !ShiftDwn && dik == DIK_V)
    {
        const char* cb_text = SDL_GetClipboardText();
        MainWindowKeyboardEvents.push_back(SDL_KEYDOWN);
        MainWindowKeyboardEvents.push_back(511);
        MainWindowKeyboardEventsText.push_back(cb_text);
        MainWindowKeyboardEvents.push_back(SDL_KEYUP);
        MainWindowKeyboardEvents.push_back(511);
        MainWindowKeyboardEventsText.push_back(cb_text);
    }
    else if (dik == DIK_CLIPBOARD_PASTE)
    {
        string text = dik_text;
        EraseInvalidChars(text, flags);
        if (!text.empty())
        {
            uint text_len_utf8 = _str(text).lengthUtf8();
            uint erase_len_utf8 = 0;
            if (str_len_utf8 + text_len_utf8 > max)
                erase_len_utf8 = str_len_utf8 + text_len_utf8 - max;

            uint text_pos = (uint)text.length();
            while (erase_len_utf8)
            {
                text_pos--;
                while (text_pos && (text[text_pos] & 0xC0) == 0x80)
                    text_pos--;
                erase_len_utf8--;
            }
            text.erase(text_pos);

            str.insert(pos, text);
            pos += (uint)text.length();
        }
    }
    // Text input
    else
    {
        if (dik_text_len_utf8 == 0)
            return;
        if (str_len_utf8 + dik_text_len_utf8 > max)
            return;
        if (CtrlDwn)
            return;

        for (size_t i = 0; i < dik_text.length();)
        {
            uint length;
            if (IsInvalidChar(dik_text.c_str() + i, flags, length))
                return;
            i += length;
        }

        str.insert(pos, dik_text);
        pos += (uint)dik_text.length();
    }
}

void Keyboard::EraseInvalidChars(string& str, int flags)
{
    for (size_t i = 0; i < str.length();)
    {
        uint length;
        if (IsInvalidChar(str.c_str() + i, flags, length))
            str.erase(i, length);
        else
            i += length;
    }
}

bool Keyboard::IsInvalidChar(const char* str, uint flags, uint& length)
{
    uint ucs = utf8::Decode(str, &length);
    if (!utf8::IsValid(ucs))
        return false;

    if (length == 1)
    {
        if (flags & KIF_NO_SPEC_SYMBOLS && (*str == '\n' || *str == '\r' || *str == '\t'))
            return true;
        if (flags & KIF_ONLY_NUMBERS && !(*str >= '0' && *str <= '9'))
            return true;
        if (flags & KIF_FILE_NAME)
        {
            switch (*str)
            {
            case '\\':
            case '/':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
            case '\n':
            case '\r':
            case '\t':
                return true;
            default:
                break;
            }
        }
    }

    return !sprMngr.HaveLetter(-1, ucs);
}

uchar Keyboard::MapKey(ushort code)
{
    return KeysMapUser[KeysMap[code]];
}

ushort Keyboard::UnmapKey(uchar key)
{
    return KeysMapRevert[key];
}
