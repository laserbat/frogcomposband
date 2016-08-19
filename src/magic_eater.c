#include "angband.h"

#include <assert.h>

#define _MAX_SLOTS 10
#define _INVALID_SLOT -1

static object_type _wands[_MAX_SLOTS];
static object_type _staves[_MAX_SLOTS];
static object_type _rods[_MAX_SLOTS];

static void _birth(void)
{
    int i;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        memset(&_wands[i], 0, sizeof(object_type));
        memset(&_staves[i], 0, sizeof(object_type));
        memset(&_rods[i], 0, sizeof(object_type));
    }
}

static object_type *_which_list(int tval)
{
    switch (tval)
    {
    case TV_WAND: return _wands;
    case TV_STAFF: return _staves;
    case TV_ROD: return _rods;
    }
    assert(0);
    return NULL;
}

static object_type *_which_obj(int tval, int slot)
{
    assert (0 <= slot && slot < _MAX_SLOTS);
    return _which_list(tval) + slot;
}

static cptr _which_name(int tval)
{
    switch (tval)
    {
    case TV_WAND: return "Wand";
    case TV_STAFF: return "Staff";
    case TV_ROD: return "Rod";
    }
    assert(0);
    return NULL;
}

static void _display(object_type *list, rect_t display)
{
    char    buf[MAX_NLEN];
    int     i;
    point_t pos = rect_topleft(&display);
    int     padding, max_o_len = 20;
    doc_ptr doc = NULL;


    padding = 5;   /* leading " a) " + trailing " " */
    padding += 12; /* " Fail: 23.2%" */

    /* Measure */
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = list + i;
        if (o_ptr->k_idx)
        {
            int len;
            object_desc(buf, o_ptr, 0);
            len = strlen(buf);
            if (len > max_o_len)
                max_o_len = len;
        }
    }

    if (max_o_len + padding > display.cx)
        max_o_len = display.cx - padding;

    /* Display */
    doc = doc_alloc(display.cx);
    doc_insert(doc, "<style:table>");
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = list + i;

        doc_printf(doc, " %c) ", I2A(i));

        if (o_ptr->k_idx)
        {
            int  fail = device_calc_fail_rate(o_ptr);

            object_desc(buf, o_ptr, OD_COLOR_CODED);
            doc_insert(doc, buf);

            if (fail == 1000)
                doc_printf(doc, "<tab:%d>Fail: %3d%%\n", display.cx - 12, fail/10);
            else
                doc_printf(doc, "<tab:%d>Fail: %2d.%d%%\n", display.cx - 12, fail/10, fail%10);

            /*doc_printf(doc, "<tab:%d>SP: %3d.%2.2d\n", display.cx - 12, o_ptr->xtra5 / 100, o_ptr->xtra5 % 100);*/
        }
        else
            doc_insert_text(doc, TERM_L_DARK, "(Empty)\n");
    }
    doc_insert(doc, "</style>");
    doc_sync_term(doc, doc_range_all(doc), doc_pos_create(pos.x, pos.y));
    doc_free(doc);
}

#define _ALLOW_EMPTY    0x01 /* Absorb */
#define _ALLOW_SWITCH   0x02 /* Browse/Use */
#define _ALLOW_EXCHANGE 0x04
object_type *_choose(cptr verb, int tval, int options)
{
    object_type *result = NULL;
    int          slot = 0;
    int          cmd;
    rect_t       display = ui_menu_rect();
    int          which_tval = tval;
    string_ptr   prompt = NULL;
    bool         done = FALSE;
    bool         exchange = FALSE;
    int          slot1 = _INVALID_SLOT, slot2 = _INVALID_SLOT;

    if ((options & _ALLOW_SWITCH) && REPEAT_PULL(&cmd))
    {
        switch (cmd)
        {
        case 'w': which_tval = TV_WAND; break;
        case 's': which_tval = TV_STAFF; break;
        case 'r': which_tval = TV_ROD; break;
        }

        if (REPEAT_PULL(&cmd))
        {
            slot = A2I(cmd);
            if (0 <= slot && slot < _MAX_SLOTS)
                return _which_obj(which_tval, slot);
        }
    }

    if (display.cx > 80)
        display.cx = 80;

    prompt = string_alloc();
    screen_save();
    while (!done)
    {
        string_clear(prompt);

        if (exchange)
        {
            if (slot1 == _INVALID_SLOT)
                string_printf(prompt, "Select the first %s:", _which_name(which_tval));
            else
                string_printf(prompt, "Select the second %s:", _which_name(which_tval));
        }
        else
        {
            string_printf(prompt, "%s which %s", verb, _which_name(which_tval));
            if (options & _ALLOW_SWITCH)
            {
                switch (which_tval)
                {
                case TV_WAND: string_append_s(prompt, " [Press 'S' for Staves, 'R' for Rods"); break;
                case TV_STAFF: string_append_s(prompt, " [Press 'W' for Wands, 'R' for Rods"); break;
                case TV_ROD: string_append_s(prompt, " [Press 'W' for Wands, 'S' for Staves"); break;
                }
                if (options & _ALLOW_EXCHANGE)
                    string_append_s(prompt, ", 'X' to Exchange");
                string_append_s(prompt, "]:");
            }
            else
                string_append_c(prompt, ':');
        }
        prt(string_buffer(prompt), 0, 0);
        _display(_which_list(which_tval), display);

        cmd = inkey_special(FALSE);

        if (cmd == ESCAPE || cmd == 'q' || cmd == 'Q')
            done = TRUE;

        if (options & _ALLOW_SWITCH)
        {
            if (cmd == 'w' || cmd == 'W')
                which_tval = TV_WAND;
            else if (cmd == 's' || cmd == 'S')
                which_tval = TV_STAFF;
            else if (cmd == 'r' || cmd == 'R')
                which_tval = TV_ROD;
        }

        if (options & _ALLOW_EXCHANGE)
        {
            if (!exchange && (cmd == 'x' || cmd == 'X'))
            {
                exchange = TRUE;
                slot1 = slot2 = _INVALID_SLOT;
            }
        }

        if ('a' <= cmd && cmd < 'a' + _MAX_SLOTS)
        {
            slot = A2I(cmd);
            if (exchange)
            {
                if (slot1 == _INVALID_SLOT)
                    slot1 = slot;
                else
                {
                    slot2 = slot;
                    if (slot1 != slot2)
                    {
                        object_type  tmp = *_which_obj(which_tval, slot1);
                        object_type *obj1 = _which_obj(which_tval, slot1);
                        object_type *obj2 = _which_obj(which_tval, slot2);

                        *obj1 = *obj2;
                        *obj2 = tmp;
                    }
                    exchange = FALSE;
                    slot1 = slot2 = _INVALID_SLOT;
                }
            }
            else
            {
                object_type *o_ptr = _which_obj(which_tval, slot);
                if (o_ptr->k_idx || (options & _ALLOW_EMPTY))
                {
                    result = o_ptr;
                    done = TRUE;
                }
            }
        }
    }

    if (result && (options & _ALLOW_SWITCH))
    {
        switch (which_tval)
        {
        case TV_WAND: REPEAT_PUSH('w'); break;
        case TV_STAFF: REPEAT_PUSH('s'); break;
        case TV_ROD: REPEAT_PUSH('r'); break;
        }
        REPEAT_PUSH(I2A(slot));
    }

    screen_load();
    string_free(prompt);
    return result;
}

void _use_object(object_type *o_ptr)
{
    int  boost = device_power(100) - 100;
    u32b flgs[OF_ARRAY_SIZE];
    bool used = FALSE;
    int  charges = 1;

    energy_use = 100;

    obj_flags(o_ptr, flgs);
    if (have_flag(flgs, OF_SPEED))
        energy_use -= energy_use * o_ptr->pval / 10;

    if (!fear_allow_device())
    {
        msg_print("You are too scared!");
        return;
    }

    if (!device_try(o_ptr))
    {
        if (flush_failure) flush();
        msg_print("You failed to use the device properly.");
        sound(SOUND_FAIL);
        return;
    }

    if (device_sp(o_ptr) < o_ptr->activation.cost)
    {
        if (flush_failure) flush();
        msg_print("The device has no charges left.");
        return;
    }

    if (o_ptr->activation.type == EFFECT_IDENTIFY)
        device_available_charges = device_sp(o_ptr) / o_ptr->activation.cost;

    sound(SOUND_ZAP);
    used = device_use(o_ptr, boost);

    if (o_ptr->activation.type == EFFECT_IDENTIFY)
        charges = device_used_charges;

    if (used)
    {
        stats_on_use(o_ptr, charges);
        device_decrease_sp(o_ptr, o_ptr->activation.cost * charges);
    }
    else
        energy_use = 0;
}

void magic_eater_browse(void)
{
    object_type *o_ptr = _choose("Browse", TV_WAND, _ALLOW_SWITCH | _ALLOW_EXCHANGE);
    if (o_ptr)
        obj_display(o_ptr);
}

void magic_eater_cast(int tval)
{
    object_type *o_ptr;

    /* Duplicate anti-magic checks since "device" commands might re-route here (as "magic" commands)
       For example, do_cmd_use_staff() will allow magic-eaters to invoke staff based spells. */
    if (dun_level && (d_info[dungeon_type].flags1 & DF1_NO_MAGIC))
    {
        msg_print("The dungeon absorbs all attempted magic!");
        return;
    }
    else if (p_ptr->tim_no_spells)
    {
        msg_print("Your spells are blocked!");
        return;
    }
    else if (p_ptr->anti_magic)
    {
        msg_print("An anti-magic shell disrupts your magic!");
        return;
    }
    else if (IS_SHERO())
    {
        msg_print("You cannot think clearly!");
        return;
    }

    if (p_ptr->confused)
    {
        msg_print("You are too confused!");
        return;
    }

    if (!tval)
        tval = TV_WAND;

    o_ptr = _choose("Use", tval, _ALLOW_SWITCH);
    if (o_ptr)
        _use_object(o_ptr);
}

/* Absorb Magic */
static bool gain_magic(void)
{
    int item;
    object_type *src_ptr;
    object_type *dest_ptr;
    char o_name[MAX_NLEN];

    item_tester_hook = object_is_device;
    if (!get_item(&item, "Gain power of which item? ", "You have nothing to gain power from.", (USE_INVEN | USE_FLOOR)))
        return FALSE;

    if (item >= 0)
        src_ptr = &inventory[item];
    else
        src_ptr = &o_list[0 - item];

    dest_ptr = _choose("Replace", src_ptr->tval, _ALLOW_EMPTY);
    if (!dest_ptr)
        return FALSE;

    if (dest_ptr->k_idx)
    {
        char prompt[255];
        object_desc(o_name, dest_ptr, OD_COLOR_CODED);
        sprintf(prompt, "Really replace %s? <color:y>[y/N]</color>", o_name);
        if (msg_prompt(prompt, "ny", PROMPT_DEFAULT) == 'n')
            return FALSE;
    }

    object_desc(o_name, src_ptr, OD_COLOR_CODED);
    msg_format("You absorb magic of %s.", o_name);

    *dest_ptr = *src_ptr;

    dest_ptr->inscription = 0;
    obj_identify_fully(dest_ptr);
    stats_on_identify(dest_ptr);

    /* Eliminate the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -999);
        inven_item_describe(item);
        inven_item_optimize(item);
    }
    /* Eliminate the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -999);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }
    return TRUE;
}

static void _absorb_magic_spell(int cmd, variant *res)
{
    switch (cmd)
    {
    case SPELL_NAME:
        var_set_string(res, "Absorb Magic");
        break;
    case SPELL_DESC:
        var_set_string(res, "");
        break;
    case SPELL_CAST:
        var_set_bool(res, gain_magic());
        break;
    default:
        default_spell(cmd, res);
        break;
    }
}

void magic_eater_gain(void)
{
    if (cast_spell(_absorb_magic_spell))
        energy_use = 100;
}

/* Regeneration */
int magic_eater_regen_amt(int tval)
{
   int amt = 3; /* per mill */

    if (p_ptr->regen > 100)
        amt += (p_ptr->regen - 100) / 100;

    if (tval == TV_ROD)
        amt *= 5;

    return amt;
}

static void _do_regen(int tval)
{
    int i;
    int base = magic_eater_regen_amt(tval);

    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = _which_obj(tval, i);
        if (o_ptr->k_idx)
        {
            int  amt = base;
            u32b flgs[OF_ARRAY_SIZE];

            obj_flags(o_ptr, flgs);
            if (have_flag(flgs, OF_REGEN))
                amt += o_ptr->pval * base / 5;

            device_regen_sp_aux(o_ptr, amt);
        }
    }
}

bool magic_eater_regen(int pct)
{
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return FALSE;

    _do_regen(TV_WAND);
    _do_regen(TV_STAFF);
    _do_regen(TV_ROD);

    return TRUE;
}

void magic_eater_restore(void)
{
    int i;
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = _which_obj(TV_WAND, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 350);
        o_ptr = _which_obj(TV_STAFF, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 350);
        o_ptr = _which_obj(TV_ROD, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 700);
    }
}

void magic_eater_restore_all(void)
{
    int i;
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = _which_obj(TV_WAND, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 1000);
        o_ptr = _which_obj(TV_STAFF, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 1000);
        o_ptr = _which_obj(TV_ROD, i);
        if (o_ptr->k_idx) device_regen_sp_aux(o_ptr, 1000);
    }
}

/* Old annoyance of Magic Eaters: No automatic resting to regenerate charges!
   See dungeon.c:process_player() */
bool magic_eater_can_regen(void)
{
    int i;
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return FALSE;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = _which_obj(TV_WAND, i);
        if (o_ptr->k_idx && device_sp(o_ptr) < device_max_sp(o_ptr))
            return TRUE;
        o_ptr = _which_obj(TV_STAFF, i);
        if (o_ptr->k_idx && device_sp(o_ptr) < device_max_sp(o_ptr))
            return TRUE;
        o_ptr = _which_obj(TV_ROD, i);
        if (o_ptr->k_idx && device_sp(o_ptr) < device_max_sp(o_ptr))
            return TRUE;
    }
    return FALSE;
}

/* Auto-ID */
bool magic_eater_auto_id(object_type *o_ptr)
{
    int i;
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return FALSE;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *device_ptr = _which_obj(TV_STAFF, i);
        if (device_ptr->activation.type == EFFECT_IDENTIFY && device_sp(device_ptr) > device_ptr->activation.cost)
        {
            identify_item(o_ptr);
            stats_on_use(device_ptr, 1);
            device_decrease_sp(device_ptr, device_ptr->activation.cost);
            return TRUE;
        }
    }
    return FALSE;
}

bool magic_eater_auto_detect_traps(void)
{
    int i;
    if (p_ptr->pclass != CLASS_MAGIC_EATER) return FALSE;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *device_ptr = _which_obj(TV_STAFF, i);
        if (device_ptr->activation.type == EFFECT_DETECT_TRAPS && device_sp(device_ptr) > device_ptr->activation.cost)
        {
            detect_traps(DETECT_RAD_DEFAULT, TRUE);
            stats_on_use(device_ptr, 1);
            device_decrease_sp(device_ptr, device_ptr->activation.cost);
            return TRUE;
        }
        device_ptr = _which_obj(TV_ROD, i);
        if (device_ptr->activation.type == EFFECT_DETECT_TRAPS && device_sp(device_ptr) > device_ptr->activation.cost)
        {
            detect_traps(DETECT_RAD_DEFAULT, TRUE);
            stats_on_use(device_ptr, 1);
            device_decrease_sp(device_ptr, device_ptr->activation.cost);
            return TRUE;
        }
        else if (device_ptr->activation.type == EFFECT_DETECT_ALL && device_sp(device_ptr) > device_ptr->activation.cost)
        {
            detect_all(DETECT_RAD_DEFAULT);
            stats_on_use(device_ptr, 1);
            device_decrease_sp(device_ptr, device_ptr->activation.cost);
            return TRUE;
        }
    }
    return FALSE;
}

/* Character Dump */
static void _dump_list(doc_ptr doc, object_type *which_list)
{
    int i;
    char o_name[MAX_NLEN];
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = which_list + i;
        if (o_ptr->k_idx)
        {
            object_desc(o_name, o_ptr, OD_COLOR_CODED);
            doc_printf(doc, "%c) %s\n", I2A(i), o_name);
        }
        else
            doc_printf(doc, "%c) (Empty)\n", I2A(i));
    }
    doc_newline(doc);
}

static void _character_dump(doc_ptr doc)
{
    doc_printf(doc, "<topic:MagicEater>================================ Absorbed <color:keypress>M</color>agic ===============================\n\n");

    _dump_list(doc, _which_list(TV_WAND));
    _dump_list(doc, _which_list(TV_STAFF));
    _dump_list(doc, _which_list(TV_ROD));

    doc_newline(doc);
}

static void _load_list(savefile_ptr file, object_type *which_list)
{
    int i;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = which_list + i;
        memset(o_ptr, 0, sizeof(object_type));
    }

    while (1)
    {
        object_type *o_ptr;
        i = savefile_read_u16b(file);
        if (i == 0xFFFF) break;
        assert(0 <= i && i < _MAX_SLOTS);
        o_ptr = which_list + i;
        rd_item(file, o_ptr);
        assert(o_ptr->k_idx);
    }
}

static void _load_player(savefile_ptr file)
{
    _load_list(file, _which_list(TV_WAND));
    _load_list(file, _which_list(TV_STAFF));
    _load_list(file, _which_list(TV_ROD));
}

static void _save_list(savefile_ptr file, object_type *which_list)
{
    int i;
    for (i = 0; i < _MAX_SLOTS; i++)
    {
        object_type *o_ptr = which_list + i;
        if (o_ptr->k_idx)
        {
            savefile_write_u16b(file, (u16b)i);
            wr_item(file, o_ptr);
        }
    }
    savefile_write_u16b(file, 0xFFFF); /* sentinel */
}

static void _save_player(savefile_ptr file)
{
    _save_list(file, _which_list(TV_WAND));
    _save_list(file, _which_list(TV_STAFF));
    _save_list(file, _which_list(TV_ROD));
}

/* Class Info */
static int _get_powers(spell_info* spells, int max)
{
    int ct = 0;

    spell_info* spell = &spells[ct++];
    spell->level = 1;
    spell->cost = 0;
    spell->fail = 0;
    spell->fn = _absorb_magic_spell;

    return ct;
}

class_t *magic_eater_get_class(void)
{
    static class_t me = {0};
    static bool init = FALSE;

    /* static info never changes */
    if (!init)
    {           /* dis, dev, sav, stl, srh, fos, thn, thb */
    skills_t bs = { 25,  42,  36,   2,  20,  16,  48,  35 };
    skills_t xs = {  7,  16,  10,   0,   0,   0,  13,  11 };

        me.name = "Magic-Eater";
        me.desc = "The Magic-Eater can absorb magical devices. Once absorbed, "
                    "these devices will function like normal objects and can be "
                    "used whenever charges are available. In effect, it is as "
                    "if the Magic-Eater had extra inventory slots for devices. "
                    "However, absorbed magic can not be drained the way normal "
                    "devices can, nor can these objects be destroyed. There are "
                    "fixed number of slots for each kind of device, and the Magic-Eater "
                    "will need to choose which object to replace once the slots are "
                    "all used. Absorbed magic can not be recharged the way normal "
                    "devices can. Instead, the Magic-Eater must rest to regain "
                    "charges the way a normal spellcaster must rest to regain "
                    "mana. Similarly, the Magic-Eater may quaff a potion to restore "
                    "mana to speed this process, though this will not necessarily "
                    "restore all of their absorbed magic.";

        me.stats[A_STR] = -1;
        me.stats[A_INT] =  2;
        me.stats[A_WIS] =  1;
        me.stats[A_DEX] =  2;
        me.stats[A_CON] = -2;
        me.stats[A_CHR] = -2;
        me.base_skills = bs;
        me.extra_skills = xs;
        me.life = 103;
        me.base_hp = 6;
        me.exp = 130;
        me.pets = 30;

        me.birth = _birth;
        me.get_powers = _get_powers;
        me.character_dump = _character_dump;
        me.load_player = _load_player;
        me.save_player = _save_player;
        init = TRUE;
    }

    return &me;
}
