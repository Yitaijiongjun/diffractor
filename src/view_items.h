// This file is part of the Diffractor photo and video organizer
// Copyright 2026  Zac Walker
// 
// This program is free software; you can redistribute it and / or modify it
// License details are available at https://www.gnu.org/licenses/lgpl-2.1.html
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY

// Purpose: Main thumbnail grid and list view. Displays file collections
// with thumbnail rendering, selection, and navigation. Layout calculates one set of
// disjoint regions (sidebar, chrome, splitters, scroll bars, media, items) that render,
// hit testing and wheel routing all share. The media column's arrangement is a free function so
// its stability contract can be tested without a window. The commands that decide what the list
// contains - the filter box, thumbnail size, type filters, totals and grouping - are declared for the
// frame's status bar, which keeps them on screen however far the list scrolls.

#pragma once

#include "ui_view.h"
#include "ui_controls.h"

class display_state_t;
class sidebar_host;
class items_view;
class folder_tree;
struct metadata_block;

// Shared by the view and every metadata listing it builds. A listing can outlive the rebuild that
// replaced it -- a latched controller still holds one -- so the posture map is refcounted and the
// view drops the hook as it goes, leaving a stranded listing inert rather than dangling.
struct metadata_tree_state
{
	std::map<std::string, bool, std::less<>> expanded;
	std::function<void()> invalidate;
};

using metadata_tree_state_ptr = std::shared_ptr<metadata_tree_state>;


// The items preview media column: the media and its primary properties, then the detail that the
// file's own metadata blocks fill in later. Free of the view so its arrangement is testable without a
// window.
struct media_column_inputs
{
	const std::vector<view_element_ptr>* priority = nullptr;
	const std::vector<view_element_ptr>* detail = nullptr;
	const std::vector<view_element_ptr>* all = nullptr;
	recti bounds;
	// The picture in the block, if there is one. The column budgets how far it may be reduced, so
	// naming it is what lets the arrangement bound the information instead of the media.
	view_element_ptr media;
};

// Returns the scrollable content height.
int layout_media_column(const media_column_inputs& in, ui::measure_context& mc, ui::control_layouts& positions);


enum class workspace_pane { folders, contents, preview };
using workspace_order = std::array<workspace_pane, 3>;

struct workspace_geometry
{
	std::array<recti, 3> panes;
	std::array<recti, 2> dividers;
	workspace_order order;
	int count = 0;
	int minimum_width = 0;
	int available_width = 0;
};

workspace_geometry arrange_workspace(sizei extent, int gap, int preferred_minimum,
                                     const settings_t::workspace_layout& layout, bool folders_visible);

struct item_and_group
{
	df::item_group_ptr g;
	df::item_element_ptr i;

	friend auto operator<=>(const item_and_group&, const item_and_group&) = default;
};

class items_view final : public view_base
{
public:
	using this_type = items_view;

	view_state& _state;
	view_host_ptr _host;

	std::vector<view_element_ptr> _item_elements;
	std::vector<view_element_ptr> _media_elements;
	std::vector<view_element_ptr> _media_priority_elements;
	std::vector<view_element_ptr> _media_detail_elements;

	// Which metadata rows the user has opened or closed. Keyed by block and stable row id so the
	// posture survives the element rebuild that every selection change performs.
	metadata_tree_state_ptr _metadata_tree = std::make_shared<metadata_tree_state>();

	view_element_ptr _media_element;
	df::item_element_ptr _layout_center_item;
	df::zoom_view_state _touch_pan_start_zoom;
	bool _touch_pan_zoom_active = false;

	view_scroller _items_scroller;
	view_scroller _media_scroller;
	view_scroll_anchor _pending_items_anchor;
	view_scroll_anchor _pending_media_anchor;
	bool _reset_media_scroll = false;

	sizei _client_extent;
	display_state_ptr _display;
	int _scroll_width = 20;
	int _header_height = 0;
	int _header_action_width = 0;
	int _divider_width = 0;
	int _pane_minimum = 0;
	int _drop_slot = -1;
	bool _show_collection = false;
	workspace_geometry _workspace;
	std::array<recti, 3> _pane_headers;
	std::shared_ptr<folder_tree> _folder_tree;

	// locations.md 7.3: an optional control row that can shrink to a one-line header when the
	// control area would otherwise bury the items the user came to see.
	struct control_row
	{
		std::shared_ptr<view_elements> element;
		view_element_ptr header;
		std::vector<view_element_ptr> content;

		void reset()
		{
			element.reset();
			header.reset();
			content.clear();
		}

		void collapsed(const bool v) const
		{
			if (header) header->is_visible(v);
			for (const auto& c : content) c->is_visible(!v);
		}
	};

	control_row _breakdown_row;

	// An explicit expansion outranks the budget's guess and lasts the session.
	bool _breakdown_expanded = false;

	// locations.md 6.3 / 7.2: the two derived control-bar rows. Both are rebuilt from published
	// worker results, so neither ever reads the index or the gazetteer while building.

	view_element_ptr build_location_breakdown_row();
	view_element_ptr build_related_header_row() const;
	void apply_control_row_budget(ui::measure_context& mc, int width, int budget);

	// Every interactive area of the view is a rectangle in this set. update_regions() is the only
	// writer and runs during layout; render, hit testing, wheel routing and context menus all read
	// it, so what is drawn, what is clicked and what is scrolled can never disagree. Regions are
	// hit tested in the fixed priority order listed here.
	struct view_regions
	{
		recti sidebar;
		recti items_scroll;
		recti media_scroll;
		recti media;
		recti items;
	};

	// The answer region_at gives, in the same priority order. Named so the wheel and the context menu
	// consume the walk rather than each writing their own and drifting from it.
	enum class view_region
	{
		none,
		header,
		divider,
		sidebar,
		items_scroll,
		media_scroll,
		media,
		items
	};

	view_region region_at(pointi loc) const;

	view_regions _regions;

	std::shared_ptr<sidebar_host> _sidebar;


	std::vector<item_and_group> _visible_items;
	// Reused by render so a frame over a large collection performs no allocation.
	std::vector<item_and_group> _draw_items;
	bool _visible_items_valid = false;
	double _next_thumbnail_retry = 0.0;

	items_view(view_state& s, view_host_ptr host);
	~items_view() override;
	const std::shared_ptr<sidebar_host>& sidebar() const { return _sidebar; }

	void activate(sizei extent) override;
	void deactivate() override;
	void refresh() override;

	void render(ui::draw_context& dc, view_controller_ptr controller) override;

	menu_type context_menu(pointi loc) override;
	// A press anywhere but on the panel's document gives Copy and Select all back to the items.
	void mouse_down(pointi loc) override;
	void release_document() const;

	void update_visible_items_list();
	void stage_visible_thumbnails();
	void retry_visible_thumbnails(double time_now);
	void layout(ui::measure_context& mc, sizei extent) override;
	bool mouse_wheel(pointi loc, ui::wheel_notch notch) override;
	bool media_wheel(pointi loc, ui::wheel_notch notch);
	bool step_zoom(pointi loc, int steps);
	bool pinch(pointi loc, int steps) override;

	bool is_caption_area(pointi loc) const override;
	view_bars bars() const override;

	void items_scroll_popup(view_hover_element& hover, pointi loc) const;
	group_and_item scroll_loc_to_item(pointi loc) const;

	// recti::contains is inclusive, so an empty (all-zero) region would otherwise claim the origin.
	static bool region_hit(const recti r, const pointi loc)
	{
		return !r.is_empty() && r.contains(loc);
	}

	// Deliberately the loose test rather than region_at: the scroll bar is drawn inside the grid's
	// rectangle, and a file dropped on it is meant for the folder the grid is showing. The strict
	// answer belongs to hit testing and the context menu, which have to tell the scroll bar apart from
	// the items under it.
	bool is_over_items(const pointi loc) const override
	{
		return region_hit(_regions.items, loc);
	}

	bool sidebar_visible() const;
	void resize_workspace(int divider, int x);
	void move_workspace_pane(workspace_pane pane, int slot);
	void toggle_workspace_collection();
	void refresh_workspace_folders();
	recti workspace_header_action(int slot) const;
	void draw_workspace_chrome(ui::draw_context& dc) const;

	void pan_start(const pointi start_loc) override
	{
		_touch_pan_zoom_active = false;
		if (!_display) return;

		const auto region = region_at(start_loc);
		if (region != view_region::media && region != view_region::media_scroll) return;
		if (!_display->zoom()) return;

		_display->active_zoom_pane_at(pointd(start_loc));
		_touch_pan_start_zoom = _display->zoom_state();
		_touch_pan_zoom_active = true;
	}

	void pan(const pointi start_loc, const pointi current_loc) override
	{
		if (_touch_pan_zoom_active && _display && _display->zoom())
			_display->pan_zoom(pointd(current_loc - start_loc), _touch_pan_start_zoom);
	}

	void pan_end(const pointi start_loc, const pointi final_loc) override
	{
		pan(start_loc, final_loc);
		_touch_pan_zoom_active = false;
	}

	recti calc_items_bounds() const
	{
		return _regions.items;
	}

	recti calc_logical_items_bounds() const
	{
		return _regions.items.offset(_items_scroller.scroll_offset());
	}

	recti calc_media_bounds() const
	{
		return _regions.media;
	}
	void update_regions();
	recti region_bounds(view_region region) const;
	view_controller_ptr media_controller_from_location(const view_host_ptr& host, pointi loc, hit_test_context& ctx);
	view_controller_ptr items_controller_from_location(const view_host_ptr& host, pointi loc, hit_test_context& ctx);
	view_controller_ptr controller_from_location(const view_host_ptr& host, pointi loc, hit_test_context& ctx) override;

	void broadcast_event(const view_element_event& event) const override;

	void make_visible(const df::item_element_ptr& i);
	bool is_visible(const df::item_element_ptr& i) const;
	void update_item_scroller_sections();
	void items_changed(bool path_changed) override;
	void display_changed() override;
	void update_media_elements() override;
	bool escape() override;
	void add_metadata_elements(std::vector<view_element_ptr>& elements, const metadata_block& block);
	void add_stream_elements(std::vector<view_element_ptr>& elements, const display_state_ptr& display,
	                         const df::item_element_ptr& item);
	void add_description_elements(std::vector<view_element_ptr>& elements, const df::item_element_ptr& item,
	                              const prop::item_metadata_const_ptr& md);


	void line_up(bool toggle_selection, bool extend_selection) const;
	void line_down(bool toggle_selection, bool extend_selection) const;
};
