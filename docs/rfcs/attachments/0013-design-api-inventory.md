# RFC 0013 design API consumer inventory

Audited src, tests and benchmarks on 2026-10-13. Native builds enforce atomic caller migration. There are no Role-indexed arrays or ordinal-cast consumers of the removed Role tail. The retained Role values 0..24 do not change.

## Field and lookup consumers outside palette definitions

```text
src/ui/messages/message_shutdown.cppm:50: Color msg_color = loom::ui::design::theme::current_theme().palette->text_muted;
src/ui/messages/collapsed_content_message.cppm:130: | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/system_text_message.cppm:128: auto summary = text(data_.summary) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted);
src/ui/messages/system_text_message.cppm:135: | dim | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/system_text_message.cppm:158: | color(loom::ui::design::theme::current_theme().palette->text_muted)
src/ui/messages/local_command_output_message.cppm:239: left.push_back(text("[f] unfold ") | color(loom::ui::design::theme::current_theme().palette->text_link) | dim);
src/ui/messages/local_command_output_message.cppm:241: left.push_back(text("[f] fold ") | color(loom::ui::design::theme::current_theme().palette->text_link) | dim);
src/ui/messages/local_command_output_message.cppm:270: text(d.working_dir) | dim | color(loom::ui::design::theme::current_theme().palette->text_link),
src/ui/messages/tool_use_message.cppm:357: buttons.push_back(pill("o Open", pal.text_link));
src/ui/messages/message_components.cppm:22: ftxui::Color fg = loom::ui::design::theme::current_theme().palette->text_link) {
src/ui/messages/api_error_message.cppm:152: parts.push_back(text(" · " + e.provider) | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/api_error_message.cppm:214: rows.push_back(text(countdown_text) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/api_error_message.cppm:221: ids.push_back(text(val) | color(loom::ui::design::theme::current_theme().palette->text_link) | bold);
src/ui/messages/api_error_message.cppm:252: bts.push_back(pill_button("q", "Dismiss", loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/message_compact_boundary.cppm:58: text("  " + *data.summary) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted),
src/ui/messages/thinking_message.cppm:212: el = el | color(loom::ui::design::theme::current_theme().palette->text_muted);
src/ui/messages/thinking_message.cppm:334: parts.push_back(text("Reasoning") | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/thinking_message.cppm:375: text(summary) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted),
src/ui/messages/thinking_message.cppm:623: | color(loom::ui::design::theme::current_theme().palette->text_muted) | xflex;
src/ui/messages/thinking_message.cppm:646: }) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted);
src/ui/messages/message_tool_result.cppm:631: text("User rejected Loom's plan:") | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/attachment_message.cppm:117: return k == AttachmentKind::Memory ? pal.remember : pal.icon_default;
src/ui/messages/attachment_message.cppm:199: meta.push_back(text("  " + right_side) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted));
src/ui/messages/user_text_message.cppm:219: | (hovered_ ? bgcolor(loom::ui::design::theme::current_theme().palette->surface_selected) : nothing)
src/ui/messages/message_image.cppm:169: text((std::string(loom::ui::design::figures::kSquare) + " ")) | color(loom::ui::design::theme::current_theme().palette->icon_default),
src/ui/messages/message_image.cppm:170: text(label) | bold | color(loom::ui::design::theme::current_theme().palette->text_link) | hyperlink(file_url),
src/ui/messages/message_image.cppm:172: : text("  " + meta_suffix) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted),
src/ui/messages/message_image.cppm:176: text((std::string(loom::ui::design::figures::kSquare) + " ")) | color(loom::ui::design::theme::current_theme().palette->icon_default),
src/ui/messages/message_image.cppm:177: text(label) | bold | color(loom::ui::design::theme::current_theme().palette->icon_default),
src/ui/messages/message_image.cppm:179: : text("  " + meta_suffix) | dim | color(loom::ui::design::theme::current_theme().palette->text_muted),
src/ui/messages/message_image.cppm:211: text(display_source) | color(loom::ui::design::theme::current_theme().palette->text_link) |
src/ui/messages/message_image.cppm:263: | (hovered_ ? bgcolor(loom::ui::design::theme::current_theme().palette->surface_selected) : nothing)
src/ui/features/plugins/plugin_install_flow.cppm:200: "Install from a downloaded .zip or a local folder", pal.text_accent},
src/ui/features/plugins/plugin_install_flow.cppm:300: : (r.install_scope == "project" ? pal.text_accent : pal.text_muted)),
src/ui/features/plugins/plugin_manage_panel.cppm:148: if (s == "project") return pal.text_accent;
src/ui/features/plugins/plugin_manage_panel.cppm:149: if (s == "local")   return pal.text_muted;
src/ui/foundation/theme_provider.cppm:322: if (color == "rainbow_red_shimmer")    return p.rainbow_shimmer_stops[0];
src/ui/foundation/theme_provider.cppm:323: if (color == "rainbow_orange_shimmer") return p.rainbow_shimmer_stops[1];
src/ui/foundation/theme_provider.cppm:324: if (color == "rainbow_yellow_shimmer") return p.rainbow_shimmer_stops[2];
src/ui/foundation/theme_provider.cppm:325: if (color == "rainbow_green_shimmer")  return p.rainbow_shimmer_stops[3];
src/ui/foundation/theme_provider.cppm:326: if (color == "rainbow_blue_shimmer")   return p.rainbow_shimmer_stops[4];
src/ui/foundation/theme_provider.cppm:327: if (color == "rainbow_indigo_shimmer") return p.rainbow_shimmer_stops[5];
src/ui/foundation/theme_provider.cppm:328: if (color == "rainbow_violet_shimmer") return p.rainbow_shimmer_stops[6];
src/ui/foundation/theme_provider.cppm:351: if (color == "rainbow_red")    return p.rainbow[0];
src/ui/foundation/theme_provider.cppm:352: if (color == "rainbow_orange") return p.rainbow[1];
src/ui/foundation/theme_provider.cppm:353: if (color == "rainbow_yellow") return p.rainbow[2];
src/ui/foundation/theme_provider.cppm:354: if (color == "rainbow_green")  return p.rainbow[3];
src/ui/foundation/theme_provider.cppm:355: if (color == "rainbow_blue")   return p.rainbow[4];
src/ui/foundation/theme_provider.cppm:356: if (color == "rainbow_indigo") return p.rainbow[5];
src/ui/foundation/theme_provider.cppm:357: if (color == "rainbow_violet") return p.rainbow[6];
src/ui/permissions/permission_rule_list.cpp:72: return pal.text_muted;
src/ui/permissions/permission_rule_list.cpp:244: Color c = loom::ui::design::theme::current_theme().palette->text_muted;
src/ui/permissions/permission_rule_list.cpp:530: desc_body.push_back(text("   " + l) | color(pal.text_muted));
src/ui/permissions/permission_rule_list.cpp:1143: ? text(" DEFAULT ") | dim | color(pal.text_muted)
src/ui/permissions/permission_rule_list.cpp:1280: : (i == 1) ? pal.danger   : pal.text_muted;
src/ui/permissions/permission_rule_list.cpp:1497: | color(pal.text_muted) | dim,
src/ui/permissions/permission_rule_list.cpp:1568: | color(pal.text_muted),
src/ui/permissions/permission_rule_list.cpp:2502: return text(" DEFAULT ") | color(pal.text_muted)
src/ui/permissions/permission_rule_list.cpp:2513: return text(" DEFAULT ") | color(pal.text_muted)
Role ordinal candidate: src/ui/permissions/permission_rules_ui.cppm:43: inline constexpr std::array<dt::Role, 4> kPermTabRoles = {
src/ui/permissions/permission_single_prompt.cppm:319: return vbox(all) | color(pal.text_muted);
src/ui/permissions/permission_single_prompt.cppm:462: return pal.text_muted;
src/ui/permissions/permission_scope_editor.cppm:138: return pal.text_muted;
src/ui/permissions/permissions_components.cppm:125: case ActionKind::Other:       return pal.text_muted;
src/ui/permissions/permissions_components.cppm:504: els.push_back(text(std::format("… and {} more", remaining)) | dim | color(pal.text_muted));
src/ui/visual/code_highlight.cppm:947: result = result | bgcolor(loom::ui::design::theme::current_theme().palette->surface_selected);
src/ui/visual/structured_diff.cppm:567: result = result | bgcolor(loom::ui::design::theme::current_theme().palette->surface_selected);
src/ui/visual/markdown_render_impl.cpp:505: color(loom::ui::design::theme::current_theme().palette->text_link);
src/ui/dialogs/settings_dialog.cppm:228: text(*description) | dim | color(pal.text_muted),
src/ui/dialogs/settings_dialog.cppm:376: text(subtitle) | dim | color(pal.text_muted),
Role ordinal candidate: src/ui/dialogs/settings_dialog.cppm:1148: auto show_toast = [state](std::string msg, loom::ui::design::tokens::Role role = loom::ui::design::tokens::Role::Success) {
src/ui/dialogs/session_picker.cppm:152: row = row | bgcolor(pal.surface_selected) | color(pal.text);
src/ui/dialogs/cost_threshold_dialog.cppm:174: vbox(std::move(body_rows)) | color(pal.text_muted),
src/ui/dialogs/quick_open.cppm:173: row = row | bgcolor(pal.surface_selected) | color(pal.text);
src/ui/dialogs/wizard_dialog.cppm:388: hints.push_back(text("←/B:back") | dim | color(pal.text_muted));
src/ui/dialogs/wizard_dialog.cppm:390: hints.push_back(text("→/N/Enter:next") | dim | color(pal.text_muted));
src/ui/dialogs/wizard_dialog.cppm:392: hints.push_back(text("Esc:cancel") | dim | color(pal.text_muted));
src/ui/dialogs/plugin_dialog.cppm:197: return loom::ui::design::theme::current_theme().palette->icon_accent;
src/ui/dialogs/plugin_dialog.cppm:231: paragraph(std::string{ui::k_plugin_help_text}) | color(pal.text_muted),
src/ui/dialogs/plugin_dialog.cppm:246: text(row.label) | bold | color(selected ? pal.text : pal.text_muted),
src/ui/dialogs/plugin_dialog.cppm:257: sub.push_back(paragraph("   " + row.message) | dim | color(pal.text_muted));
src/ui/dialogs/plugin_dialog.cppm:328: paragraph(m) | (ok ? color(pal.text_muted) : color(pal.danger) | dim),
src/ui/widgets/text_input_render.cpp:389: rows.push_back(separator() | color(pal.border_default));
src/ui/widgets/text_input_render.cpp:443: | color(pal.border_default);
src/ui/widgets/tag_tabs.cppm:74: label = label | bold | color(pal.text) | bgcolor(pal.surface_selected);
src/ui/widgets/tag_tabs.cppm:113: label = label | bold | color(pal.text) | bgcolor(pal.surface_selected);
tests/ui/test_theme_screens_visual.cpp:100: ExpectWordColor(Render(loom::ui::render_markdown("`code` ![image](https://example.test) $math$")), "image", pal.text_link);
tests/ui/test_message_interactions.cpp:46: EXPECT_EQ(screen.PixelAt(1, 0).foreground_color, theme::current_theme().palette->text_link);
tests/ui/test_theme_dialogs_permissions.cpp:168: EXPECT_EQ(color, pal.icon_accent);
tests/ui/test_theme_messages_prompt.cpp:194: ExpectWordColor(card, "manual.pdf", theme::current_theme().palette->icon_default);
tests/ui/test_theme_messages_prompt.cpp:266: ExpectWordColor(user_screen, "HoverBody", pal.text, pal.surface_selected);
tests/ui/test_theme_messages_prompt.cpp:269: ExpectWordColor(image_screen, "capture.png", pal.text_link, pal.surface_selected);
Role ordinal candidate: tests/core/test_bootstrap.cpp:278: msg.role = static_cast<h::Role>(i % 5);
Role ordinal candidate: tests/services/test_mcp_client.cpp:1765: EXPECT_EQ(prompt->messages[0].role, loom::services::mcp::PromptRole::User);
Role ordinal candidate: tests/services/test_mcp_client.cpp:1767: EXPECT_EQ(prompt->messages[1].role, loom::services::mcp::PromptRole::Assistant);
```

The rainbow string-key mappings above use arrays and remain unchanged. No benchmark consumes the removed API. Source symbol audit finds no external consumers of the removed primitive closure. Keep kbd (messages/message_components.cppm) and spinner_glyph (widgets/passes.cppm, three arguments).

Ordinal-search candidates above are not consumers of the deleted Role tail:
kPermTabRoles is a four-entry array of retained semantic values, indexed by tab,
not a table indexed by Role ordinals. The settings-dialog line is a typed
default function parameter. Bootstrap uses history Role and MCP tests use
PromptRole, separate enums from UI design Role.
