#include "wx/joyedit.h"

#include <wx/tokenzr.h>

#include "opts.h"
#include "strutils.h"
#include "wx/userinput.h"

// FIXME: suppport analog/digital flag on per-axis basis

IMPLEMENT_DYNAMIC_CLASS(wxJoyKeyTextCtrl, wxKeyTextCtrl)

BEGIN_EVENT_TABLE(wxJoyKeyTextCtrl, wxKeyTextCtrl)
EVT_SDLJOY(wxJoyKeyTextCtrl::OnJoy)
END_EVENT_TABLE()

int wxJoyKeyTextCtrl::DigitalButton(const wxJoyEvent& event)
{
    int16_t sdlval = event.control_value();
    wxJoyControl sdltype = event.control();

    switch (sdltype) {
    case wxJoyControl::Axis:
        // for val = 0 return arbitrary direction; val means "off"
        return sdlval > 0 ? WXJB_AXIS_PLUS : WXJB_AXIS_MINUS;

    case wxJoyControl::Hat:

        /* URDL = 1248 */
        switch (sdlval) {
        case 1:
            return WXJB_HAT_N;

        case 2:
            return WXJB_HAT_E;

        case 3:
            return WXJB_HAT_NE;

        case 4:
            return WXJB_HAT_S;

        case 6:
            return WXJB_HAT_SE;

        case 8:
            return WXJB_HAT_W;

        case 9:
            return WXJB_HAT_NW;

        case 12:
            return WXJB_HAT_SW;

        default:
            return WXJB_HAT_N; // arbitrary direction; val = 0 means "off"
        }

    case wxJoyControl::Button:
        return WXJB_BUTTON;

    default:
        // unknown ctrl type
        return -1;
    }
}

void wxJoyKeyTextCtrl::OnJoy(wxJoyEvent& event)
{
    static wxLongLong last_event = 0;

    // Filter consecutive axis motions within 300ms, as this adds two bindings
    // +1/-1 instead of the one intended.
    if (event.control() == wxJoyControl::Axis && wxGetUTCTimeMillis() - last_event < 300)
        return;

    last_event = wxGetUTCTimeMillis();

    if (!event.control_value() || DigitalButton(event) < 0)
        return;

    wxString nv = wxUserInput::FromJoyEvent(event).ToString();

    if (nv.empty())
        return;

    if (multikey) {
        wxString ov = GetValue();

        if (!ov.empty())
            nv = ov + multikey + nv;
    }

    SetValue(nv);

    if (keyenter)
        Navigate();
}

wxString wxJoyKeyTextCtrl::ToString(int mod, int key, int joy, bool isConfig)
{
    if (!joy)
        return wxKeyTextCtrl::ToString(mod, key, isConfig);

    wxString s;
    // Note: wx translates unconditionally (2.8.12, 2.9.1)!
    // So any strings added below must also be translated unconditionally
    s.Printf(("Joy%d-"), joy);
    wxString mk;

    switch (mod) {
    case WXJB_AXIS_PLUS:
        mk.Printf(("Axis%d+"), key);
        break;

    case WXJB_AXIS_MINUS:
        mk.Printf(("Axis%d-"), key);
        break;

    case WXJB_BUTTON:
        mk.Printf(("Button%d"), key);
        break;

    case WXJB_HAT_N:
        mk.Printf(("Hat%dN"), key);
        break;

    case WXJB_HAT_S:
        mk.Printf(("Hat%dS"), key);
        break;

    case WXJB_HAT_W:
        mk.Printf(("Hat%dW"), key);
        break;

    case WXJB_HAT_E:
        mk.Printf(("Hat%dE"), key);
        break;

    case WXJB_HAT_NW:
        mk.Printf(("Hat%dNW"), key);
        break;

    case WXJB_HAT_NE:
        mk.Printf(("Hat%dNE"), key);
        break;

    case WXJB_HAT_SW:
        mk.Printf(("Hat%dSW"), key);
        break;

    case WXJB_HAT_SE:
        mk.Printf(("Hat%dSE"), key);
        break;
    }

    s += mk;
    return s;
}

wxString wxJoyKeyTextCtrl::FromAccelToString(wxAcceleratorEntry_v keys, wxChar sep, bool isConfig)
{
    wxString ret;

    for (size_t i = 0; i < keys.size(); i++) {
        if (i > 0)
            ret += sep;

        wxString key = ToString(keys[i].GetFlags(), keys[i].GetKeyCode(), keys[i].GetJoystick(), isConfig);

        if (key.empty())
            return wxEmptyString;

        ret += key;
    }

    return ret;
}

#include <wx/regex.h>

// only parse regex once
// Note: wx translates unconditionally (2.8.12, 2.9.1)!
// So any strings added below must also be translated unconditionally
// \1 is joy #
static wxRegEx joyre;
// \1 is axis# and \2 is + or -
static wxRegEx axre;
// \1 is button#
static wxRegEx butre;
// \1 is hat#, \3 is N, \4 is S, \5 is E, \6 is W, \7 is NE, \8 is SE,
// \9 is SW, \10 is NW
static wxRegEx hatre;
// use of static wxRegeEx is not thread-safe
static wxCriticalSection recs;

// wx provides no atoi for wxChar
// this is not a universal function; assumes valid number
static int simple_atoi(const wxString& s, int len)
{
    int ret = 0;

    for (int i = 0; i < len; i++)
        ret = ret * 10 + (int)(s[i] - wxT('0'));

    return ret;
}

static void CompileRegex()
{
    // \1 is joy #
    joyre.Compile(("^Joy([0-9]+)[-+]"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is axis# and \2 is + or -
    axre.Compile(("Axis([0-9]+)([+-])"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is button#
    butre.Compile(("Button([0-9]+)"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is hat#, \3 is N, \4 is S, \5 is E, \6 is W, \7 is NE,
    // \8 is SE, \9 is SW, \10 is NW
    hatre.Compile(("Hat([0-9]+)((N|North|U|Up)|(S|South|D|Down)|"
                    "(E|East|R|Right)|(W|West|L|Left)|"
                    "(NE|NorthEast|UR|UpRight)|(SE|SouthEast|DR|DownRight)|"
                    "(SW|SouthWest|DL|DownLeft)|(NW|NorthWest|UL|UpLeft))"),
                  wxRE_EXTENDED | wxRE_ICASE);
}

static bool ParseJoy(const wxString& s, int len, int& mod, int& key, int& joy)
{
    mod = key = joy = 0;

    if (!len)
        return false;

    wxCriticalSectionLocker lk(recs);
    size_t b, l;

    CompileRegex();
    if (!joyre.Matches(s) || !joyre.GetMatch(&b, &l) || b)
        return false;

    const wxString p = s.Mid(l);
    size_t alen = len - l;
    joyre.GetMatch(&b, &l, 1);
    joy = simple_atoi(s.Mid(b), l);
#define is_ctrl(re) re.Matches(p) && re.GetMatch(&b, &l) && l == alen && !b

    if (is_ctrl(axre)) {
        axre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        axre.GetMatch(&b, &l, 2);
        mod = p[b] == wxT('+') ? WXJB_AXIS_PLUS : WXJB_AXIS_MINUS;
    } else if (is_ctrl(butre)) {
        butre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        mod = WXJB_BUTTON;
    } else if (is_ctrl(hatre)) {
        hatre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
#define check_dir(n, d) if (hatre.GetMatch(&b, &l, n) && l > 0) mod = WXJB_HAT_##d

        check_dir(3, N);
        else check_dir(4, S);
        else check_dir(5, E);
        else check_dir(6, W);
        else check_dir(7, NE);
        else check_dir(8, SE);
        else check_dir(9, SW);
        else check_dir(10, NW);
    } else {
        joy = 0;
        return false;
    }

    return true;
}

bool wxJoyKeyTextCtrl::ParseString(const wxString& s, int len, int& mod, int& key, int& joy)
{
    if (ParseJoy(s, len, mod, key, joy))
        return true;

    return wxKeyTextCtrl::ParseString(s, len, mod, key);
}

bool wxJoyKeyTextCtrl::FromString(const wxString& s, int& mod, int& key, int& joy)
{
    return ParseString(s, s.size(), mod, key, joy);
}

wxAcceleratorEntry_v wxJoyKeyTextCtrl::ToAccelFromString(const wxString& s, wxChar sep)
{
    wxAcceleratorEntry_v ret, empty;
    int mod, key, joy;
    if (s.size() == 0)
        return empty;

    for (const auto& token : str_split_with_sep(s, sep)) {
        if (!ParseString(token, token.size(), mod, key, joy))
            return empty;
        ret.insert(ret.begin(), wxAcceleratorEntryUnicode(token, joy, mod, key));
    }
    return ret;
}

IMPLEMENT_CLASS(wxJoyKeyValidator, wxValidator)

bool wxJoyKeyValidator::TransferToWindow()
{
    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    jk->SetValue(wxUserInput::SpanToString(gopts.game_control_bindings[val_]));
    return true;
}

bool wxJoyKeyValidator::TransferFromWindow()
{
    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    gopts.game_control_bindings[val_] = wxUserInput::FromString(jk->GetValue());
    return true;
}
