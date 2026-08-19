#include <stdio.h>
#include <string.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_topology.h>

static henka_authoring_topology_profile parse_profile(
    const char* value)
{
    if (value != NULL &&
        strcmp(value, "organic") == 0)
    {
        return
            HENKA_AUTHORING_TOPOLOGY_PROFILE_ORGANIC;
    }

    if (value != NULL &&
        strcmp(value, "hard_surface") == 0)
    {
        return
            HENKA_AUTHORING_TOPOLOGY_PROFILE_HARD_SURFACE;
    }

    return
        HENKA_AUTHORING_TOPOLOGY_PROFILE_GENERAL;
}

static const char* profile_label(
    henka_authoring_topology_profile profile)
{
    if (profile ==
        HENKA_AUTHORING_TOPOLOGY_PROFILE_ORGANIC)
    {
        return "ORGANIC";
    }

    if (profile ==
        HENKA_AUTHORING_TOPOLOGY_PROFILE_HARD_SURFACE)
    {
        return "HARD_SURFACE";
    }

    return "GENERAL";
}

int main(int argc, char** argv)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_topology_options options =
        henka_authoring_topology_options_default();
    henka_authoring_topology_report report;
    henka_authoring_topology_profile profile;
    henka_authoring_topology_profile_guidance guidance;
    const char* quad_status;

    if (argc != 3)
    {
        fprintf(
            stderr,
            "usage: authoring_topology_report <general|organic|hard_surface> <source.hams>\n");

        return 2;
    }

    profile =
        parse_profile(argv[1]);

    guidance =
        henka_authoring_topology_profile_get_guidance(
            profile);

    if (henka_authoring_mesh_load_file_new(
            argv[2],
            &mesh) != HENKA_SUCCESS ||
        mesh == NULL)
    {
        fprintf(
            stderr,
            "failed to load authoring source: %s\n",
            argv[2]);

        henka_authoring_mesh_destroy(mesh);
        return 1;
    }

    if (henka_authoring_topology_analyze(
            mesh,
            &options,
            &report) != HENKA_SUCCESS)
    {
        fprintf(
            stderr,
            "failed to analyze authoring source: %s\n",
            argv[2]);

        henka_authoring_mesh_destroy(mesh);
        return 1;
    }

    quad_status =
        report.face_count == 0U ||
        report.quad_face_ratio >=
            guidance.recommended_minimum_quad_ratio
            ? "MEETS"
            : "BELOW";

    printf(
        "TOPOLOGY_REPORT "
        "profile=%s "
        "path=%s "
        "vertices=%zu "
        "edges=%zu "
        "faces=%zu "
        "triangles=%zu "
        "quads=%zu "
        "ngons=%zu "
        "quad_ratio=%.4f "
        "recommended_quad_ratio=%.4f "
        "quad_recommendation=%s "
        "components=%zu "
        "isolated_vertices=%zu "
        "boundary_edges=%zu "
        "nonmanifold_edges=%zu "
        "hard_edges=%zu "
        "uv_seams=%zu "
        "inconsistent_winding_edges=%zu "
        "degenerate_faces=%zu "
        "duplicate_faces=%zu "
        "coincident_vertex_pairs=%zu "
        "valence_le2=%zu "
        "valence3=%zu "
        "valence4=%zu "
        "valence5plus=%zu "
        "max_valence=%zu "
        "max_face_corners=%zu\n",
        profile_label(profile),
        argv[2],
        report.vertex_count,
        report.edge_count,
        report.face_count,
        report.triangle_count,
        report.quad_count,
        report.ngon_count,
        report.quad_face_ratio,
        guidance.recommended_minimum_quad_ratio,
        quad_status,
        report.connected_component_count,
        report.isolated_vertex_count,
        report.boundary_edge_count,
        report.nonmanifold_edge_count,
        report.hard_edge_count,
        report.uv_seam_edge_count,
        report.inconsistent_winding_edge_count,
        report.degenerate_face_count,
        report.duplicate_face_count,
        report.coincident_vertex_pair_count,
        report.valence_two_or_less_vertex_count,
        report.valence_three_vertex_count,
        report.valence_four_vertex_count,
        report.valence_five_plus_vertex_count,
        report.max_vertex_valence,
        report.max_face_corners);

    henka_authoring_mesh_destroy(mesh);
    return 0;
}