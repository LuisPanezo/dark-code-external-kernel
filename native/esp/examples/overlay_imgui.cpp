/*
 * Overlay ESP para ImGui — archivo de referencia listo para pegar.
 *
 * Por defecto dibuja SOLO las líneas (finas, con degradado) desde la caja
 * hasta el ancla elegida en pantalla. El combo elige el ancla:
 *   Arriba  = (w/2, 0)      Centro = (w/2, h/2)      Abajo = (w/2, h)
 *
 *   EspOverlayOptions esp;   // esp.op2 es el interruptor
 *   DrawEspOverlay(w, h, esp);
 *   DrawEspPanel(esp);
 *
 * Requisitos: root en el proceso y /dev/phmeop cargado. El hilo de dibujo sólo
 * proyecta; la memoria la lee el hilo interno del servicio.
 */
#if __has_include(<imgui.h>)

#include <imgui.h>
#include <cstdio>

#include "esp/api/esp_api.h"

/*
 * Opciones del overlay. `op2` es el interruptor (el checkbox se llama igual).
 */
struct EspOverlayOptions {
    bool  op2{false};            // ESP encendido/apagado
    bool  only_lines{true};      // sólo líneas (sin caja, nombre ni vida)
    int   line_anchor{2};        // ancla en pantalla: 0 arriba, 1 centro, 2 abajo
    int   line_position{1};      // salida en la caja: 0 arriba, 1 medio, 2 abajo
    float line_thickness{1.0f};  // fina por defecto
    bool  names{true};
    bool  distances{true};
    bool  health_bars{true};
};

namespace {

constexpr int   kCapacity = 64;
constexpr ImU32 kEnemy    = IM_COL32(255, 96, 96, 235);
constexpr ImU32 kTeammate = IM_COL32(110, 235, 160, 235);
constexpr ImU32 kShadow   = IM_COL32(0, 0, 0, 170);
constexpr ImU32 kText     = IM_COL32(255, 255, 255, 240);
constexpr ImU32 kHealthOk = IM_COL32(90, 220, 140, 235);
constexpr ImU32 kHealthLo = IM_COL32(255, 170, 60, 235);
constexpr ImU32 kHealthBad= IM_COL32(230, 60, 60, 235);

/* Salida de la línea sobre la caja, igual que linePosition del ejemplo. */
ImVec2 LineOffset(const DcEspBox& box, int line_position) {
    switch (line_position) {
    case 0:  return ImVec2(box.x + box.w * 0.5f, box.y + 5.0f);
    case 1:  return ImVec2(box.x + box.w * 0.5f, box.y + box.h * 0.5f);
    case 2:  return ImVec2(box.x + box.w * 0.5f, box.y + box.h - 5.0f);
    default: return ImVec2(box.x + box.w * 0.5f, box.y + box.h * 0.5f);
    }
}

/* Ancla de la línea en pantalla: arriba, centro o abajo. */
ImVec2 LineAnchor(float screen_width, float screen_height, int line_anchor) {
    switch (line_anchor) {
    case 0:  return ImVec2(screen_width * 0.5f, 0.0f);
    case 1:  return ImVec2(screen_width * 0.5f, screen_height * 0.5f);
    case 2:  return ImVec2(screen_width * 0.5f, screen_height);
    default: return ImVec2(screen_width * 0.5f, screen_height);
    }
}

/*
 * Línea VERDE, notable: borde oscuro por debajo para que resalte sobre
 * cualquier fondo, y núcleo verde brillante que se intensifica hacia el ancla.
 */
void DrawSnapline(ImDrawList* draw, const DcEspBox& box, const EspOverlayOptions& options,
                  float screen_width, float screen_height) {
    const ImVec2 origin = LineOffset(box, options.line_position);
    const ImVec2 anchor = LineAnchor(screen_width, screen_height, options.line_anchor);

    constexpr int kSegments = 12;
    for (int index = 0; index < kSegments; ++index) {
        const float from = static_cast<float>(index) / kSegments;
        const float to = static_cast<float>(index + 1) / kSegments;
        const ImVec2 a{origin.x + (anchor.x - origin.x) * from,
                       origin.y + (anchor.y - origin.y) * from};
        const ImVec2 b{origin.x + (anchor.x - origin.x) * to,
                       origin.y + (anchor.y - origin.y) * to};

        // Borde oscuro (más grueso) → recorta la línea sobre cielo/claro.
        draw->AddLine(a, b, IM_COL32(0, 0, 0, 150), options.line_thickness + 2.0f);
        // Núcleo verde: 120 → 255, bien visible.
        const int alpha = 120 + static_cast<int>(135.0f * to);
        draw->AddLine(a, b, IM_COL32(0, 255, 120, alpha), options.line_thickness);
    }
}

void DrawBox(ImDrawList* draw, const DcEspBox& box, float font_size,
             const EspOverlayOptions& options) {
    const ImVec2 top{box.x, box.y};
    const ImVec2 bottom{box.x + box.w, box.y + box.h};
    const ImU32 color = (box.flags & DC_ESP_FLAG_TEAMMATE) ? kTeammate : kEnemy;

    const float corner = (box.w < 24.f ? box.w : 24.f) * 0.25f;
    draw->AddRect(top, bottom, IM_COL32(0, 0, 0, 90), 0.f, 0, 3.f);
    draw->AddRect(top, bottom, color, 0.f, 0, 1.2f);
    draw->AddLine({top.x, top.y}, {top.x + corner, top.y}, color, 2.f);
    draw->AddLine({top.x, top.y}, {top.x, top.y + corner}, color, 2.f);
    draw->AddLine({bottom.x - corner, bottom.y}, {bottom.x, bottom.y}, color, 2.f);
    draw->AddLine({bottom.x, bottom.y - corner}, {bottom.x, bottom.y}, color, 2.f);

    if (options.health_bars && !(box.flags & DC_ESP_FLAG_NO_HEALTH)) {
        float ratio = box.health / 100.f;
        if (ratio < 0.f) ratio = 0.f;
        if (ratio > 1.f) ratio = 1.f;
        const float bar_w = 3.f;
        const ImVec2 bar_min{top.x - bar_w - 2.f, top.y};
        const ImVec2 bar_max{top.x - 2.f, bottom.y};
        draw->AddRectFilled(bar_min, bar_max, IM_COL32(0, 0, 0, 160));
        const ImU32 health_color = ratio > 0.5f ? kHealthOk : (ratio > 0.2f ? kHealthLo : kHealthBad);
        const float filled = (bar_max.y - bar_min.y) * ratio;
        draw->AddRectFilled({bar_min.x, bar_max.y - filled}, bar_max, health_color);
    }

    if (options.names || options.distances) {
        char label[96];
        const char* name = box.name[0] ? box.name : "";
        if (options.names && options.distances) {
            std::snprintf(label, sizeof(label), "%s  %.0fm", name, box.distance_m);
        } else if (options.names) {
            std::snprintf(label, sizeof(label), "%s", name);
        } else {
            std::snprintf(label, sizeof(label), "%.0fm", box.distance_m);
        }
        const ImVec2 text_pos{top.x, top.y - font_size - 3.f};
        draw->AddText({text_pos.x + 1.f, text_pos.y + 1.f}, kShadow, label);
        draw->AddText(text_pos, kText, label);
    }
}

} // namespace

/* Dibuja el overlay y devuelve cuántos objetivos pintó. */
int DrawEspOverlay(float screen_width, float screen_height, EspOverlayOptions& options) {
    dc_esp_set_enabled(options.op2 ? 1 : 0);
    if (!options.op2) return 0;

    static DcEspBox boxes[kCapacity];
    const int count = dc_esp_poll(screen_width, screen_height, boxes, kCapacity);
    if (count <= 0) return 0;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float font_size = ImGui::GetFontSize();
    for (int index = 0; index < count; ++index) {
        DrawSnapline(draw, boxes[index], options, screen_width, screen_height);
        if (!options.only_lines) DrawBox(draw, boxes[index], font_size, options);
    }
    return count;
}

/* Envoltorio para quien ya llamaba con bool*. */
int DrawEspOverlay(float screen_width, float screen_height, bool* enabled) {
    EspOverlayOptions options;
    options.op2 = enabled != nullptr && *enabled;
    const int count = DrawEspOverlay(screen_width, screen_height, options);
    if (enabled) *enabled = options.op2;
    return count;
}

/* Panel de control: interruptor `op2`, ancla de las líneas y estado. */
void DrawEspPanel(EspOverlayOptions& options) {
    if (!ImGui::Begin("ESP", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (ImGui::Checkbox("op2", &options.op2)) {
        dc_esp_set_enabled(options.op2 ? 1 : 0);
    }
    ImGui::SameLine();
    ImGui::Text("%s", options.op2 ? "ESP activo" : "ESP apagado");

    ImGui::Separator();
    ImGui::TextUnformatted("Lineas");
    static const char* kAnchors[] = {"Arriba", "Centro", "Abajo"};
    ImGui::Combo("Anclaje", &options.line_anchor, kAnchors, 3);

    ImGui::TextUnformatted("Salida en la caja");
    ImGui::RadioButton("Arriba##salida", &options.line_position, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Medio##salida", &options.line_position, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Abajo##salida", &options.line_position, 2);

    ImGui::SliderFloat("Grosor", &options.line_thickness, 0.5f, 3.0f, "%.1f px");
    ImGui::Checkbox("Solo lineas", &options.only_lines);

    if (!options.only_lines) {
        ImGui::Separator();
        ImGui::TextUnformatted("Caja");
        ImGui::Checkbox("Nombres", &options.names);
        ImGui::SameLine();
        ImGui::Checkbox("Distancia", &options.distances);
        ImGui::Checkbox("Barra de vida", &options.health_bars);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Lector");
    ImGui::Text("%s", dc_esp_status());
    ImGui::Text("%d objetivos", dc_esp_target_count());
    if (ImGui::Button("Reconectar")) {
        dc_esp_detach();
        DcEspConfig config{};
        config.package = "com.garena.game.df";
        config.min_health = 1;
        config.skip_teammates = 1;
        dc_esp_attach_config(&config);
    }
    ImGui::End();
}

/* Envoltorio para quien ya llamaba con bool*. */
void DrawEspPanel(bool* enabled) {
    static EspOverlayOptions options;
    options.op2 = enabled != nullptr && *enabled;
    DrawEspPanel(options);
    if (enabled) *enabled = options.op2;
}

#endif // __has_include(<imgui.h>)
