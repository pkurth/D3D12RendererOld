#pragma once


enum keyboard_key
{
	key_0, key_1, key_2, key_3, key_4, key_5, key_6, key_7, key_8, key_9,
	key_a, key_b, key_c, key_d, key_e, key_f, key_g, key_h, key_i, key_j,
	key_k, key_l, key_m, key_n, key_o, key_p, key_q, key_r, key_s, key_t,
	key_u, key_v, key_w, key_x, key_y, key_z,
	key_space, key_enter, key_tab, key_esc,
	key_up, key_down, key_left, key_right,
	key_backspace, key_delete,

	key_shift, key_alt, key_ctrl,

	key_count, key_unknown
};

enum mouse_button
{
	mouse_left,
	mouse_right,
	mouse_middle,
	mouse_4,
	mouse_5,
};

struct keyboard_event
{
	keyboard_key key;
	bool shiftDown, ctrlDown, altDown;
};

struct character_event
{
	uint32 codePoint;
};

struct mouse_button_event
{
	mouse_button button;
	uint32 x, y;
	float relX, relY;
	bool shiftDown, ctrlDown, altDown;
};

struct mouse_move_event
{
	uint32 x, y;
	float relX, relY;
	float relDX, relDY;
	bool leftDown, rightDown, middleDown;
	bool shiftDown, ctrlDown, altDown;
};

struct mouse_scroll_event
{
	float scroll;
	uint32 x, y;
	float relX, relY;
	bool leftDown, rightDown, middleDown;
	bool shiftDown, ctrlDown, altDown;
};

