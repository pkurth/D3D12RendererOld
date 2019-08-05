#pragma once

enum kb_key
{
	key_0, key_1, key_2, key_3, key_4, key_5, key_6, key_7, key_8, key_9,
	key_a, key_b, key_c, key_d, key_e, key_f, key_g, key_h, key_i, key_j,
	key_k, key_l, key_m, key_n, key_o, key_p, key_q, key_r, key_s, key_t,
	key_u, key_v, key_w, key_x, key_y, key_z,
	key_space, key_enter, key_tab, key_esc,
	key_up, key_down, key_left, key_right,
	key_backspace, key_delete,

	key_shift = (1 << 28), key_alt = (1 << 29), key_ctrl = (1 << 30),

	key_count = key_delete + 3, key_unknown
};

enum mouse_button
{
	mouse_left,
	mouse_right,
	mouse_middle,
	mouse_4,
	mouse_5,
};

enum event_type
{
	event_type_down,
	event_type_up,

	// Mouse only.
	event_type_scroll,
	event_type_move,
	event_type_enter_window,
	event_type_leave_window,
};

struct key_input_event
{
	kb_key key;
	event_type type;
	uint32 modifiers; // Shift, alt oder ctrl.
};

struct mouse_input_event
{
	mouse_button button;
	event_type type;
	uint32 modifiers;
	uint32 x, y;
	float scroll;
};
