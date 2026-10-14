#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiShowcase.h"

namespace oryx::gui
{

namespace
{

constexpr uint32_t k_tab_count = 4;
const char* const k_tabs[k_tab_count] = { "Widgets", "Inputs", "Data", "Style" };
const std::string_view k_names[] = { "Tic-Tac-Toe", "Hexapawn", "Connect Four", "Nim", "Checkers", "Go", "Chess", "Othello", "Gomoku", "Backgammon" };
const std::string_view k_games[] = { "Tic-Tac-Toe", "Hexapawn", "Connect Four" };
constexpr uint32_t k_list_items = 400;
constexpr uint32_t k_table_rows = 1000;
constexpr uint32_t k_wave = 96;
constexpr uint32_t k_cells = 9;

// Widget state is at most 64 bytes per id, so the demo keeps a few small records, copied out at the start of the call and back at the end.
struct Basics
{
    float slider = 0.5f;
    float drag = 1.0f;
    int32_t radio = 0;
    int32_t count = 3;
    int32_t combo = 0;
    uint32_t tab = 0;
    uint32_t cell = k_no_index;
    int32_t table_row = -1;
    int32_t edit_row = -1;
    bool flag = true;
    bool disable = false;
    bool cell_edit = false;
    bool show_tab[k_tab_count] = { true, true, true, true };
    bool ascending = true;
};

struct Entry
{
    char text[64] = "Hello, Oryx";
};

struct Numbers
{
    char filter[32] = {};
    float number = 1.5f;
    int32_t integer = 7;
    float entry_slider = 0.25f;
    float entry_drag = 12.0f;
};

struct Picks
{
    uint64_t words[selection_words(k_list_items)] = {};
    Selection selection;
};

struct Clock
{
    float time = 0.0f;
    float edited = 0.0f;
};

static_assert(sizeof(Basics) <= k_max_widget_state_size && sizeof(Entry) <= k_max_widget_state_size && sizeof(Numbers) <= k_max_widget_state_size && sizeof(Picks) <= k_max_widget_state_size);

uint32_t hash_of(uint32_t index)
{
    return index * 2654435761u;
}

float value_of(uint32_t ply)
{
    return static_cast<float>(hash_of(ply) % 2001u) / 1000.0f - 1.0f;
}

uint32_t visits_of(uint32_t ply)
{
    return hash_of(ply) % 9973u;
}

struct Showcase
{
    Basics basics;
    Entry entry;
    Numbers numbers;
    Picks picks;
    Clock clock;
    float wave[k_wave] = {};
    float policy[k_cells] = {};

    void animate(float delta_time)
    {
        clock.time += delta_time;
        for (uint32_t index = 0; index < k_wave; ++index)
        {
            const float phase = static_cast<float>(index) * 0.15f + clock.time * 1.5f;
            wave[index] = math::sin(phase) + 0.4f * math::sin(phase * 2.7f);
        }
        for (uint32_t index = 0; index < k_cells; ++index)
        {
            policy[index] = 0.5f + 0.5f * math::sin(clock.time * 0.8f + static_cast<float>(index));
        }
    }

    void widgets();
    void inputs();
    void data();
    void style();
};

void Showcase::widgets()
{
    if (collapsing_header("basics", true))
    {
        label("A label");
        bullet("A bullet");
        const ItemState button_state = button("Toast");
        if (button_state.clicked)
        {
            toast("Hello from the GUI", 2.0f);
        }
        tooltip(button_state, "Shows a message for two seconds");
        checkbox("checkbox", basics.flag);
        radio("first", basics.radio, 0);
        radio("second", basics.radio, 1);
        badge("badge", { 0.0f, 0.62f, 0.45f, 1.0f });
        progress("progress", basics.slider, {});
    }
    if (collapsing_header("disabled", false))
    {
        checkbox("disable the controls below", basics.disable);
        DisabledScope off(basics.disable);
        button("Button");
        checkbox("checkbox (off)", basics.flag);
        slider_float("slider (off)", basics.slider, 0.0f, 1.0f);
    }
    if (collapsing_header("input", true))
    {
        slider_float("slider", basics.slider, 0.0f, 1.0f);
        slider_int("count", basics.count, 0, 10);
        drag_float("drag", basics.drag, 0.0f, 100.0f);
        ComboScope combo("game", k_games[basics.combo]);
        for (int32_t index = 0; index < 3; ++index)
        {
            if (combo.item(k_games[index], index == basics.combo))
            {
                basics.combo = index;
            }
        }
    }
    if (collapsing_header("tree"))
    {
        TreeScope root("root", { .default_open = true });
        if (root.open())
        {
            label("leaf one");
            TreeScope branch("branch");
            if (branch.open())
            {
                label("leaf two");
            }
        }
    }
}

void Showcase::inputs()
{
    FrameArena& arena = context().arena();
    if (collapsing_header("text field", true))
    {
        const TextInputResult result = text_input("text", entry.text, sizeof(entry.text));
        key_value("length", arena.format("%u bytes%s", static_cast<uint32_t>(std::strlen(entry.text)), result.submitted ? ", submitted" : ""));
        label("Click to focus; type, arrows, shift selects, cmd+A/C/X/V, double-click selects all");
    }
    if (collapsing_header("filtering", true))
    {
        search_box("filter", numbers.filter, sizeof(numbers.filter), "Filter names");
        ScrollScope names("names", { .height = fixed(110.0f) });
        for (const std::string_view name : k_names)
        {
            if (filter_matches(numbers.filter, name))
            {
                selectable(name, false);
            }
        }
    }
    if (collapsing_header("numbers", true))
    {
        input_float("float", numbers.number, { .min = -10.0f, .max = 10.0f });
        input_int("int", numbers.integer, { .min = 0.0f, .max = 100.0f });
        slider_float("slider (double-click)", numbers.entry_slider, 0.0f, 1.0f);
        drag_float("drag (double-click)", numbers.entry_drag, 0.0f, 100.0f);
        label("Enter or a click elsewhere takes the number, Escape puts the old one back");
    }
    if (collapsing_header("multi-select list", true))
    {
        key_value("selected", arena.format("%u of %u", selection_size(picks.words, k_list_items), k_list_items));
        {
            RowScope row("select buttons", { .gap = 4.0f });
            if (small_button("all").clicked)
            {
                select_all(picks.selection, picks.words, k_list_items);
            }
            if (small_button("none").clicked)
            {
                selection_clear(picks.words, k_list_items);
            }
        }
        ListBoxScope list("list, 400 items", { .item_count = k_list_items });
        for (uint32_t index = list.first_item(); index < list.last_item(); ++index)
        {
            select_on_press(list.item(index, arena.format("item %u", index), selection_contains(picks.words, k_list_items, index)), index, picks.selection, picks.words, k_list_items);
        }
        label("Click picks one, cmd-click toggles, shift-click picks a run");
    }
}

void Showcase::data()
{
    FrameArena& arena = context().arena();
    if (collapsing_header("policy heat grid", true))
    {
        HeatGridOptions options;
        options.cell = { 40.0f, 28.0f };
        options.show_values = true;
        options.format = { NumberStyle::Fixed, 2 };
        const HeatGridResult result = heat_grid("policy", values(policy), 3, sequential_scale(), options);
        basics.cell = result.clicked_index != k_no_index ? result.clicked_index : basics.cell;
        key_value("selected cell", basics.cell == k_no_index ? std::string_view("none") : arena.format("%u", basics.cell));
    }
    if (collapsing_header("plots", true))
    {
        {
            PlotOptions options;
            options.height = fixed(90.0f);
            PlotScope plot("waves", options);
            plot.line("line", values(wave));
            plot.line("step", values_ring(wave, k_wave, 20), PlotStyle::Step);
            plot.hline(0.0f, { 0.6f, 0.6f, 0.6f, 1.0f });
            if (plot.result().hover_index != k_no_index)
            {
                plot.vline(static_cast<float>(plot.result().hover_index), { 1.0f, 1.0f, 1.0f, 0.6f });
            }
        }
        plot_histogram("distribution", values(policy), { .height = fixed(60.0f), .y_min = 0.0f, .y_max = 1.0f });
        RowScope row("sparklines", { .gap = 8.0f });
        sparkline("wave", values(wave));
        sparkline("area", values(policy), { .colour = { 0.9f, 0.62f, 0.0f, 1.0f } });
        legend_item("wave series", theme().palette[0]);
        legend_item("policy series", { 0.9f, 0.62f, 0.0f, 1.0f });
    }
    if (collapsing_header("table, 1000 rows", true))
    {
        TableOptions options;
        options.row_count = k_table_rows;
        options.height = fixed(220.0f);
        TableScope table("trace", options);
        table.column("Ply", fixed(60.0f), true);
        table.column("Value", grow(), false, TextAlign::Right);
        table.column("Visits", fixed(80.0f), false, TextAlign::Right);
        const TableResult head = table.headers();
        if (head.sort_changed)
        {
            basics.ascending = head.sort_ascending;
        }
        for (uint32_t index = table.first_row(); index < table.last_row(); ++index)
        {
            const uint32_t ply = basics.ascending ? index : k_table_rows - 1 - index;
            const ItemState row_state = table.row(index, basics.table_row == static_cast<int32_t>(ply));
            if (row_state.pressed)
            {
                basics.table_row = static_cast<int32_t>(ply);
                basics.edit_row = basics.edit_row == basics.table_row ? basics.edit_row : -1;
            }
            if (row_state.double_clicked)
            {
                basics.edit_row = static_cast<int32_t>(ply);
                basics.cell_edit = true;
                clock.edited = value_of(ply);
            }
            table.cell(arena.format("%u", ply));
            if (basics.edit_row == static_cast<int32_t>(ply))
            {
                table.begin_cell();
                input_float("##value", clock.edited, { .decimals = 3, .edit = &basics.cell_edit });
                table.end_cell();
                basics.edit_row = basics.cell_edit ? basics.edit_row : -1;
            }
            else
            {
                table.cell(arena.format("%.3f", static_cast<double>(value_of(ply))));
            }
            table.cell(arena.format("%u", visits_of(ply)));
        }
        label("Double-click a row to edit its value (the edit is not kept)");
    }
}

void Showcase::style()
{
    FrameArena& arena = context().arena();
    const GuiTheme& active = theme();
    if (collapsing_header("roles", true))
    {
        const struct
        {
            const char* name;
            const ImStyle* style;
        } roles[] = { { "panel", &active.panel }, { "header", &active.header }, { "field", &active.field }, { "button", &active.button }, { "tab", &active.tab }, { "overlay", &active.overlay } };
        for (const auto& role : roles)
        {
            colour_swatch(role.name, role.style->background);
        }
        colour_swatch("accent", active.field.accent);
        colour_swatch("selected", active.field.selected);
        key_value("text on panel", arena.format("%.1f : 1", static_cast<double>(contrast_ratio(active.panel.text, active.panel.background))));
        key_value("text on field", arena.format("%.1f : 1", static_cast<double>(contrast_ratio(active.field.text, active.field.background))));
        key_value("accent on field", arena.format("%.1f : 1", static_cast<double>(contrast_ratio(active.field.accent, active.field.background))));
    }
    if (collapsing_header("states", true))
    {
        RowScope row("buttons", { .gap = 4.0f });
        button("Rest / hover / press");
        toggle("Toggle", basics.flag);
        DisabledScope off;
        button("Disabled");
    }
    if (collapsing_header("inspector", false))
    {
        inspector();
    }
}

} // namespace

void show_showcase()
{
    Showcase demo;
    demo.basics = state<Basics>(id("showcase.basics"));
    demo.entry = state<Entry>(id("showcase.entry"));
    demo.numbers = state<Numbers>(id("showcase.numbers"));
    demo.picks = state<Picks>(id("showcase.picks"));
    demo.clock = state<Clock>(id("showcase.clock"));
    demo.animate(io().delta_time);

    {
        TabBarScope tabs("tabs", demo.basics.tab);
        for (uint32_t index = 0; index < k_tab_count; ++index)
        {
            tabs.tab(k_tabs[index], &demo.basics.show_tab[index]);
        }
    }
    uint32_t shown = demo.basics.tab < k_tab_count && demo.basics.show_tab[demo.basics.tab] ? demo.basics.tab : k_tab_count;
    for (uint32_t index = 0; index < k_tab_count && shown == k_tab_count; ++index)
    {
        shown = demo.basics.show_tab[index] ? index : k_tab_count;
    }
    {
        RowScope closed("closed tabs", { .gap = 4.0f });
        for (uint32_t index = 0; index < k_tab_count; ++index)
        {
            if (!demo.basics.show_tab[index] && small_button(k_tabs[index]).clicked)
            {
                demo.basics.show_tab[index] = true;
                shown = index;
            }
        }
    }
    separator();
    demo.basics.tab = shown;
    if (shown == k_tab_count)
    {
        label("All tabs are closed");
    }
    else
    {
        ScrollScope body(k_tabs[shown]);
        switch (shown)
        {
        case 0: demo.widgets(); break;
        case 1: demo.inputs(); break;
        case 2: demo.data(); break;
        default: demo.style(); break;
        }
    }

    state<Basics>(id("showcase.basics")) = demo.basics;
    state<Entry>(id("showcase.entry")) = demo.entry;
    state<Numbers>(id("showcase.numbers")) = demo.numbers;
    state<Picks>(id("showcase.picks")) = demo.picks;
    state<Clock>(id("showcase.clock")) = demo.clock;
}

} // namespace oryx::gui
