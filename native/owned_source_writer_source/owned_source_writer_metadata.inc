/* Generated fixed reflection; no runtime schema/AST allocation. */
static const osw_field_desc fields_11[] = {
    {"pointer", 16u, 4u, 0u, 0u},
    {"bytes", 24u, 4u, 1u, 0u},
    {"sha256", 32u, 9u, 2u, 1u},
    {"complete", 64u, 6u, 3u, 0u},
};
static const osw_field_desc fields_12[] = {
    {"kind", 16u, 1u, 0u, 0u},
    {"raw", 24u, 5u, 1u, 0u},
    {"generation", 32u, 5u, 2u, 0u},
};
static const osw_field_desc fields_13[] = {
    {"kind", 16u, 1u, 0u, 0u},
    {"raw", 24u, 4u, 1u, 0u},
    {"generation", 32u, 5u, 2u, 1u},
};
static const osw_field_desc fields_14[] = {
    {"ref", 16u, 12u, 0u, 0u},
    {"output_field", 56u, 1u, 1u, 0u},
    {"parent", 64u, 13u, 2u, 1u},
    {"owner_creator_call", 104u, 5u, 3u, 1u},
};
static const osw_field_desc fields_15[] = {
    {"command", 16u, 13u, 0u, 1u},
    {"recording_epoch_ref", 56u, 5u, 1u, 1u},
    {"resource", 64u, 13u, 2u, 1u},
    {"source_site", 104u, 1u, 3u, 1u},
    {"pipeline_owner", 112u, 13u, 4u, 1u},
    {"owner_creation_call", 152u, 5u, 5u, 1u},
    {"reset_ordinal", 160u, 5u, 6u, 1u},
    {"pipeline_route", 168u, 1u, 7u, 1u},
};
static const osw_field_desc fields_17[] = {
    {"births", 16u, 16u, 0u, 0u},
    {"recording_epoch_ref", 136u, 5u, 1u, 1u},
    {"command_buffer", 144u, 4u, 2u, 1u},
    {"pso_pointer", 152u, 4u, 3u, 1u},
    {"device_interface_pointer", 160u, 4u, 4u, 1u},
    {"device_origin", 168u, 1u, 5u, 1u},
    {"reset_ordinal", 176u, 5u, 6u, 1u},
    {"root_signature_pointer", 184u, 4u, 7u, 1u},
    {"pso_root_pointer", 192u, 4u, 8u, 1u},
};
static const osw_field_desc fields_18[] = {
    {"dimension", 16u, 3u, 0u, 0u},
    {"alignment", 24u, 4u, 1u, 0u},
    {"width", 32u, 4u, 2u, 0u},
    {"height", 40u, 1u, 3u, 0u},
    {"depth_or_array_size", 44u, 0u, 4u, 0u},
    {"mip_levels", 46u, 0u, 5u, 0u},
    {"format", 48u, 3u, 6u, 0u},
    {"sample_count", 52u, 1u, 7u, 0u},
    {"sample_quality", 56u, 1u, 8u, 0u},
    {"layout", 60u, 3u, 9u, 0u},
    {"flags", 64u, 1u, 10u, 0u},
};
static const osw_field_desc fields_19[] = {
    {"heap_type", 16u, 3u, 0u, 0u},
    {"count", 20u, 1u, 1u, 0u},
    {"flags", 24u, 1u, 2u, 0u},
    {"node_mask", 28u, 1u, 3u, 0u},
};
static const osw_field_desc fields_20[] = {
    {"format", 16u, 1u, 0u, 0u},
    {"view_dimension", 20u, 1u, 1u, 0u},
    {"component_mapping", 24u, 1u, 2u, 0u},
    {"most_detailed_mip", 28u, 1u, 3u, 0u},
    {"mip_levels", 32u, 1u, 4u, 0u},
    {"plane_slice", 36u, 1u, 5u, 0u},
    {"min_lod_clamp_bits", 40u, 1u, 6u, 0u},
};
static const osw_field_desc fields_21[] = {
    {"range_type", 16u, 3u, 0u, 0u},
    {"num_descriptors", 20u, 1u, 1u, 0u},
    {"base_shader_register", 24u, 1u, 2u, 0u},
    {"register_space", 28u, 1u, 3u, 0u},
    {"offset", 32u, 1u, 4u, 0u},
    {"flags", 36u, 1u, 5u, 1u},
};
static const osw_field_desc fields_22[] = {
    {"shader_register", 16u, 1u, 0u, 0u},
    {"register_space", 20u, 1u, 1u, 0u},
    {"num32", 24u, 1u, 2u, 0u},
};
static const osw_field_desc fields_23[] = {
    {"shader_register", 16u, 1u, 0u, 0u},
    {"register_space", 20u, 1u, 1u, 0u},
    {"flags", 24u, 1u, 2u, 1u},
};
static const osw_field_desc fields_25[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 24u, 2u, 0u},
    {"complete", 80u, 6u, 3u, 0u},
};
static const osw_field_desc fields_26[] = {
    {"parameter_type", 16u, 3u, 0u, 0u},
    {"visibility", 20u, 3u, 1u, 0u},
    {"ranges", 24u, 25u, 2u, 1u},
    {"constants", 112u, 22u, 3u, 1u},
    {"descriptor", 144u, 23u, 4u, 1u},
};
static const osw_field_desc fields_28[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 27u, 2u, 0u},
    {"complete", 568u, 6u, 3u, 0u},
};
static const osw_field_desc fields_29[] = {
    {"original_version", 16u, 3u, 0u, 0u},
    {"flags", 20u, 1u, 1u, 0u},
    {"parameters", 24u, 28u, 2u, 0u},
    {"static_sampler_count", 600u, 1u, 3u, 0u},
};
static const osw_field_desc fields_30[] = {
    {"x_bits", 16u, 1u, 0u, 0u},
    {"y_bits", 20u, 1u, 1u, 0u},
    {"width_bits", 24u, 1u, 2u, 0u},
    {"height_bits", 28u, 1u, 3u, 0u},
    {"min_depth_bits", 32u, 1u, 4u, 0u},
    {"max_depth_bits", 36u, 1u, 5u, 0u},
};
static const osw_field_desc fields_31[] = {
    {"left", 16u, 2u, 0u, 0u},
    {"top", 20u, 2u, 1u, 0u},
    {"right", 24u, 2u, 2u, 0u},
    {"bottom", 28u, 2u, 3u, 0u},
};
static const osw_field_desc fields_34[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"starts_pointer", 24u, 4u, 1u, 0u},
    {"starts", 32u, 32u, 2u, 0u},
    {"sizes_pointer", 64u, 4u, 3u, 0u},
    {"sizes", 72u, 33u, 4u, 1u},
};
static const osw_field_desc fields_35[] = {
    {"s_type", 16u, 3u, 0u, 0u},
    {"p_next", 24u, 4u, 1u, 0u},
    {"flags", 32u, 1u, 2u, 0u},
    {"stage", 36u, 3u, 3u, 0u},
    {"module", 40u, 13u, 4u, 0u},
    {"name_pointer", 80u, 4u, 5u, 0u},
    {"entry", 88u, 7u, 6u, 1u},
    {"specialization_pointer", 352u, 4u, 7u, 0u},
};
static const osw_field_desc fields_36[] = {
    {"context", 16u, 5u, 0u, 0u},
    {"run_id", 24u, 8u, 1u, 0u},
    {"enabled", 56u, 6u, 2u, 0u},
    {"faulted", 60u, 6u, 3u, 0u},
    {"gap", 64u, 6u, 4u, 0u},
    {"overflow", 68u, 6u, 5u, 0u},
    {"calls_seen", 72u, 4u, 6u, 0u},
    {"original_returns", 80u, 4u, 7u, 0u},
    {"calls_dropped", 88u, 4u, 8u, 0u},
    {"pending", 96u, 1u, 9u, 0u},
    {"clock", 104u, 4u, 10u, 0u},
    {"sealed_at", 112u, 4u, 11u, 0u},
    {"context_epoch", 120u, 5u, 12u, 0u},
};
static const osw_field_desc fields_37[] = {
    {"reset_call", 16u, 5u, 0u, 1u},
    {"emission_call", 24u, 5u, 1u, 1u},
    {"execute_call", 32u, 5u, 2u, 1u},
};
static const osw_field_desc fields_39[] = {
    {"code", 16u, 7u, 0u, 0u},
    {"calls", 280u, 38u, 1u, 0u},
};
static const osw_field_desc fields_40[] = {
    {"heap", 16u, 12u, 0u, 0u},
    {"index", 56u, 1u, 1u, 0u},
    {"cpu_handle", 64u, 4u, 2u, 0u},
};
static const osw_field_desc fields_41[] = {
    {"resource", 16u, 12u, 0u, 0u},
    {"original_view_call", 56u, 5u, 1u, 0u},
    {"history_version", 64u, 5u, 2u, 0u},
    {"lineage_calls", 72u, 38u, 3u, 0u},
    {"selected_slot", 144u, 40u, 4u, 0u},
    {"root_layout_call", 216u, 5u, 5u, 0u},
    {"root_parameter_index", 224u, 1u, 6u, 0u},
    {"format", 228u, 1u, 7u, 0u},
    {"most_detailed_mip", 232u, 1u, 8u, 0u},
    {"mip_count", 236u, 1u, 9u, 0u},
    {"declared_width", 240u, 1u, 10u, 0u},
    {"declared_height", 244u, 1u, 11u, 0u},
    {"binding_origin", 248u, 1u, 12u, 0u},
    {"cut", 256u, 5u, 13u, 0u},
};
static const osw_field_desc fields_42[] = {
    {"domain", 16u, 1u, 0u, 0u},
    {"rect", 24u, 31u, 1u, 0u},
};
static const osw_field_desc fields_43[] = {
    {"domain", 16u, 1u, 0u, 0u},
    {"value", 24u, 30u, 1u, 0u},
};
static const osw_field_desc fields_45[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 44u, 2u, 0u},
    {"complete", 104u, 6u, 3u, 0u},
};
static const osw_field_desc fields_47[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 46u, 2u, 0u},
    {"complete", 760u, 6u, 3u, 0u},
};
static const osw_field_desc fields_49[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 48u, 2u, 0u},
    {"complete", 120u, 6u, 3u, 0u},
};
static const osw_field_desc fields_51[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 50u, 2u, 0u},
    {"complete", 80u, 6u, 3u, 0u},
};
static const osw_field_desc fields_53[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 52u, 2u, 0u},
    {"complete", 72u, 6u, 3u, 0u},
};
static const osw_field_desc fields_55[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 54u, 2u, 0u},
    {"complete", 48u, 6u, 3u, 0u},
};
static const osw_field_desc fields_57[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 56u, 2u, 0u},
    {"complete", 80u, 6u, 3u, 0u},
};
static const osw_field_desc fields_58[] = {
    {"type", 16u, 3u, 0u, 0u},
    {"cpu_page_property", 20u, 3u, 1u, 0u},
    {"memory_pool_preference", 24u, 3u, 2u, 0u},
    {"creation_node_mask", 28u, 1u, 3u, 0u},
    {"visible_node_mask", 32u, 1u, 4u, 0u},
};
static const osw_field_desc fields_60[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 59u, 2u, 0u},
    {"complete", 48u, 6u, 3u, 0u},
};
static const osw_field_desc fields_62[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 61u, 2u, 0u},
    {"complete", 44u, 6u, 3u, 0u},
};
static const osw_field_desc fields_64[] = {
    {"filter", 16u, 3u, 0u, 0u},
    {"address_u", 20u, 3u, 1u, 0u},
    {"address_v", 24u, 3u, 2u, 0u},
    {"address_w", 28u, 3u, 3u, 0u},
    {"mip_lod_bias_bits", 32u, 1u, 4u, 0u},
    {"max_anisotropy", 36u, 1u, 5u, 0u},
    {"comparison_func", 40u, 3u, 6u, 0u},
    {"border_color_bits", 44u, 63u, 7u, 0u},
    {"min_lod_bits", 68u, 1u, 8u, 0u},
    {"max_lod_bits", 72u, 1u, 9u, 0u},
};
static const osw_field_desc fields_65[] = {
    {"stage_flags", 16u, 1u, 0u, 0u},
    {"offset", 20u, 1u, 1u, 0u},
    {"size", 24u, 1u, 2u, 0u},
};
static const osw_field_desc fields_67[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 66u, 2u, 0u},
    {"complete", 72u, 6u, 3u, 0u},
};
static const osw_field_desc fields_68[] = {
    {"view_mask", 16u, 1u, 0u, 0u},
    {"color_formats", 24u, 62u, 1u, 0u},
    {"depth_format", 72u, 3u, 2u, 0u},
    {"stencil_format", 76u, 3u, 3u, 0u},
};
static const osw_field_desc fields_69[] = {
    {"flags", 16u, 4u, 0u, 0u},
};
static const osw_field_desc fields_71[] = {
    {"kind", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"s_type", 32u, 3u, 2u, 0u},
    {"next_pointer", 40u, 4u, 3u, 0u},
    {"payload", 48u, 70u, 4u, 0u},
};
static const osw_field_desc fields_73[] = {
    {"pointer", 16u, 4u, 0u, 0u},
    {"nodes", 24u, 72u, 1u, 0u},
    {"complete", 288u, 6u, 2u, 0u},
};
static const osw_field_desc fields_74[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 32u, 2u, 0u},
    {"complete", 64u, 6u, 3u, 0u},
};
static const osw_field_desc fields_75[] = {
    {"structure_pointer", 16u, 4u, 0u, 0u},
    {"s_type", 24u, 3u, 1u, 0u},
    {"libraries", 32u, 74u, 2u, 0u},
};
static const osw_field_desc fields_76[] = {
    {"sampler_heap", 16u, 12u, 0u, 0u},
    {"sampler_table_call", 56u, 5u, 1u, 0u},
    {"sampler_parameter", 64u, 1u, 2u, 0u},
    {"sampler_gpu_handle", 72u, 4u, 3u, 0u},
    {"constants_call", 80u, 5u, 4u, 0u},
    {"constant_words", 88u, 59u, 5u, 0u},
    {"rtv_heap", 104u, 12u, 6u, 0u},
    {"rtv_resource", 144u, 12u, 7u, 0u},
    {"create_rtv_call", 184u, 5u, 8u, 0u},
    {"rtv_bind_call", 192u, 5u, 9u, 0u},
};
static const osw_field_desc fields_77[] = {
    {"offset_x", 16u, 2u, 0u, 0u},
    {"offset_y", 20u, 2u, 1u, 0u},
    {"width", 24u, 1u, 2u, 0u},
    {"height", 28u, 1u, 3u, 0u},
};
static const osw_field_desc fields_79[] = {
    {"count", 16u, 1u, 0u, 0u},
    {"pointer", 24u, 4u, 1u, 0u},
    {"values", 32u, 78u, 2u, 0u},
    {"complete", 72u, 6u, 3u, 0u},
};
static const osw_field_desc fields_80[] = {
    {"viewport_call", 16u, 5u, 0u, 0u},
    {"scissor_call", 24u, 5u, 1u, 0u},
    {"viewport", 32u, 30u, 2u, 0u},
    {"scissor", 72u, 77u, 3u, 0u},
};
static const osw_field_desc fields_81[] = {
    {"vertex_input", 16u, 4u, 0u, 0u},
    {"input_assembly", 24u, 4u, 1u, 0u},
    {"tessellation", 32u, 4u, 2u, 0u},
    {"viewport", 40u, 4u, 3u, 0u},
    {"rasterization", 48u, 4u, 4u, 0u},
    {"multisample", 56u, 4u, 5u, 0u},
    {"depth_stencil", 64u, 4u, 6u, 0u},
    {"color_blend", 72u, 4u, 7u, 0u},
    {"dynamic", 80u, 4u, 8u, 0u},
};
static const osw_field_desc fields_82[] = {
    {"output_pointer", 16u, 4u, 0u, 0u},
};
static const osw_field_desc fields_83[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_85[] = {
    {"physical_device_raw", 16u, 4u, 0u, 0u},
    {"output_pointer", 24u, 4u, 1u, 0u},
};
static const osw_field_desc fields_86[] = {
    {"vk_result", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_88[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"heap_properties_pointer", 56u, 4u, 1u, 0u},
    {"heap_properties", 64u, 58u, 2u, 1u},
    {"heap_flags", 104u, 1u, 3u, 0u},
    {"description_pointer", 112u, 4u, 4u, 0u},
    {"description", 120u, 18u, 5u, 1u},
    {"initial_state", 192u, 1u, 6u, 0u},
    {"clear_value_pointer", 200u, 4u, 7u, 0u},
    {"requested_iid", 208u, 10u, 8u, 1u},
    {"output_pointer", 224u, 4u, 9u, 0u},
};
static const osw_field_desc fields_89[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_91[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"description", 56u, 19u, 1u, 0u},
    {"output_pointer", 88u, 4u, 2u, 0u},
    {"description_pointer", 96u, 4u, 3u, 0u},
    {"requested_iid", 104u, 10u, 4u, 1u},
};
static const osw_field_desc fields_92[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_94[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"output_pointer", 56u, 4u, 1u, 0u},
    {"type", 64u, 3u, 2u, 0u},
};
static const osw_field_desc fields_95[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_97[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"output_pointer", 56u, 4u, 1u, 0u},
    {"type", 64u, 3u, 2u, 0u},
};
static const osw_field_desc fields_98[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_100[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"allocator", 56u, 13u, 1u, 1u},
    {"initial_pipeline", 96u, 13u, 2u, 1u},
    {"output_pointer", 136u, 4u, 3u, 0u},
    {"node_mask", 144u, 1u, 4u, 0u},
    {"type", 148u, 3u, 5u, 0u},
    {"requested_iid", 152u, 10u, 6u, 1u},
};
static const osw_field_desc fields_101[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_103[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"blob", 56u, 11u, 1u, 0u},
    {"output_pointer", 128u, 4u, 2u, 0u},
    {"node_mask", 136u, 1u, 3u, 0u},
};
static const osw_field_desc fields_104[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_106[] = {
    {"blob", 16u, 11u, 0u, 0u},
    {"description_output_pointer", 88u, 4u, 1u, 0u},
};
static const osw_field_desc fields_107[] = {
    {"status", 16u, 2u, 0u, 0u},
    {"description", 24u, 29u, 1u, 1u},
};
static const osw_field_desc fields_109[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"root", 56u, 13u, 1u, 0u},
    {"vertex_bytecode", 96u, 11u, 2u, 0u},
    {"pixel_bytecode", 168u, 11u, 3u, 0u},
    {"output_pointer", 240u, 4u, 4u, 0u},
    {"rtv_formats", 248u, 62u, 5u, 0u},
    {"sample_count", 296u, 1u, 6u, 0u},
    {"sample_quality", 300u, 1u, 7u, 0u},
};
static const osw_field_desc fields_110[] = {
    {"hresult", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_112[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"create_info_pointer", 56u, 4u, 1u, 0u},
    {"s_type", 64u, 3u, 2u, 0u},
    {"p_next", 72u, 4u, 3u, 0u},
    {"flags", 80u, 1u, 4u, 0u},
    {"code", 88u, 11u, 5u, 0u},
    {"allocator_pointer", 160u, 4u, 6u, 0u},
    {"output_pointer", 168u, 4u, 7u, 0u},
};
static const osw_field_desc fields_113[] = {
    {"vk_result", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_115[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"create_info_pointer", 56u, 4u, 1u, 0u},
    {"s_type", 64u, 3u, 2u, 0u},
    {"p_next", 72u, 4u, 3u, 0u},
    {"flags", 80u, 1u, 4u, 0u},
    {"set_layout_raws", 88u, 45u, 5u, 0u},
    {"push_constant_ranges", 200u, 67u, 6u, 0u},
    {"allocator_pointer", 280u, 4u, 7u, 0u},
    {"output_pointer", 288u, 4u, 8u, 0u},
};
static const osw_field_desc fields_116[] = {
    {"vk_result", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_118[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"cache_raw", 56u, 4u, 1u, 0u},
    {"create_info_count", 64u, 1u, 2u, 0u},
    {"create_info_pointer", 72u, 4u, 3u, 0u},
    {"create_info_complete", 80u, 6u, 4u, 0u},
    {"s_type", 84u, 3u, 5u, 0u},
    {"flags", 88u, 1u, 6u, 0u},
    {"p_next", 96u, 73u, 7u, 0u},
    {"library_info", 392u, 75u, 8u, 1u},
    {"layout", 496u, 13u, 9u, 1u},
    {"render_pass_raw", 536u, 4u, 10u, 0u},
    {"subpass", 544u, 1u, 11u, 0u},
    {"base_pipeline_raw", 552u, 4u, 12u, 0u},
    {"base_pipeline_index", 560u, 2u, 13u, 0u},
    {"stages", 568u, 47u, 14u, 0u},
    {"allocator_pointer", 1336u, 4u, 15u, 0u},
    {"output_pointer", 1344u, 4u, 16u, 0u},
    {"state_pointers", 1352u, 81u, 17u, 0u},
};
static const osw_field_desc fields_119[] = {
    {"vk_result", 16u, 2u, 0u, 0u},
    {"output_raw", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_121[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"heap_type", 56u, 3u, 1u, 0u},
};
static const osw_field_desc fields_122[] = {
    {"value", 16u, 1u, 0u, 0u},
};
static const osw_field_desc fields_124[] = {
    {"heap", 16u, 13u, 0u, 0u},
    {"output_pointer", 56u, 4u, 1u, 0u},
};
static const osw_field_desc fields_125[] = {
    {"returned_pointer", 16u, 4u, 0u, 0u},
    {"value", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_127[] = {
    {"heap", 16u, 13u, 0u, 0u},
    {"output_pointer", 56u, 4u, 1u, 0u},
};
static const osw_field_desc fields_128[] = {
    {"returned_pointer", 16u, 4u, 0u, 0u},
    {"value", 24u, 4u, 1u, 1u},
};
static const osw_field_desc fields_130[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"resource", 56u, 13u, 1u, 1u},
    {"destination", 96u, 4u, 2u, 0u},
    {"description", 104u, 20u, 3u, 1u},
    {"description_pointer", 152u, 4u, 4u, 0u},
};
static const osw_field_desc fields_133[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"count", 56u, 1u, 1u, 0u},
    {"destination", 64u, 4u, 2u, 0u},
    {"source", 72u, 4u, 3u, 0u},
    {"heap_type", 80u, 3u, 4u, 0u},
};
static const osw_field_desc fields_136[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"destinations", 56u, 34u, 1u, 0u},
    {"sources", 152u, 34u, 2u, 0u},
    {"heap_type", 248u, 3u, 3u, 0u},
};
static const osw_field_desc fields_139[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"allocator", 56u, 13u, 1u, 0u},
    {"initial_pipeline", 96u, 13u, 2u, 1u},
};
static const osw_field_desc fields_140[] = {
    {"hresult", 16u, 2u, 0u, 0u},
};
static const osw_field_desc fields_142[] = {
    {"command", 16u, 13u, 0u, 0u},
};
static const osw_field_desc fields_143[] = {
    {"hresult", 16u, 2u, 0u, 0u},
};
static const osw_field_desc fields_145[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"pipeline", 56u, 13u, 1u, 1u},
};
static const osw_field_desc fields_148[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"pipeline", 56u, 13u, 1u, 0u},
};
static const osw_field_desc fields_151[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"root", 56u, 13u, 1u, 1u},
};
static const osw_field_desc fields_154[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"heaps", 56u, 49u, 1u, 0u},
};
static const osw_field_desc fields_157[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"parameter", 56u, 1u, 1u, 0u},
    {"gpu_handle", 64u, 4u, 2u, 0u},
};
static const osw_field_desc fields_160[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"viewports", 56u, 51u, 1u, 0u},
};
static const osw_field_desc fields_163[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"rects", 56u, 53u, 1u, 0u},
};
static const osw_field_desc fields_166[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"handles", 56u, 55u, 1u, 0u},
    {"single_range", 112u, 2u, 2u, 0u},
    {"dsv_pointer", 120u, 4u, 3u, 0u},
};
static const osw_field_desc fields_169[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"vertex_count", 56u, 1u, 1u, 0u},
    {"instance_count", 60u, 1u, 2u, 0u},
    {"first_vertex", 64u, 1u, 3u, 0u},
    {"first_instance", 68u, 1u, 4u, 0u},
};
static const osw_field_desc fields_172[] = {
    {"vk_command_buffer", 16u, 4u, 0u, 0u},
    {"bind_point", 24u, 3u, 1u, 0u},
    {"pipeline", 32u, 13u, 2u, 0u},
};
static const osw_field_desc fields_175[] = {
    {"vk_command_buffer", 16u, 4u, 0u, 0u},
    {"vertex_count", 24u, 1u, 1u, 0u},
    {"instance_count", 28u, 1u, 2u, 0u},
    {"first_vertex", 32u, 1u, 3u, 0u},
    {"first_instance", 36u, 1u, 4u, 0u},
};
static const osw_field_desc fields_178[] = {
    {"queue", 16u, 13u, 0u, 0u},
    {"commands", 56u, 57u, 1u, 0u},
};
static const osw_field_desc fields_181[] = {
    {"object", 16u, 13u, 0u, 0u},
};
static const osw_field_desc fields_182[] = {
    {"value", 16u, 1u, 0u, 0u},
};
static const osw_field_desc fields_184[] = {
    {"allocator", 16u, 13u, 0u, 0u},
};
static const osw_field_desc fields_185[] = {
    {"hresult", 16u, 2u, 0u, 0u},
};
static const osw_field_desc fields_187[] = {
    {"state_pointer", 16u, 4u, 0u, 0u},
    {"dynamic_state_pointer", 24u, 4u, 1u, 0u},
    {"dsv_format_pointer", 32u, 4u, 2u, 0u},
    {"dynamic_flags_output_pointer", 40u, 4u, 3u, 0u},
};
static const osw_field_desc fields_188[] = {
    {"value", 16u, 4u, 0u, 0u},
    {"dynamic_flags", 24u, 1u, 1u, 1u},
};
static const osw_field_desc fields_190[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"description_pointer", 56u, 4u, 1u, 0u},
    {"description", 64u, 64u, 2u, 1u},
    {"description_complete", 144u, 6u, 3u, 0u},
    {"destination", 152u, 4u, 4u, 0u},
};
static const osw_field_desc fields_193[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"resource", 56u, 13u, 1u, 1u},
    {"description_pointer", 96u, 4u, 2u, 0u},
    {"destination", 104u, 4u, 3u, 0u},
};
static const osw_field_desc fields_196[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"parameter", 56u, 1u, 1u, 0u},
    {"words", 64u, 60u, 2u, 0u},
    {"destination_offset", 120u, 1u, 3u, 0u},
};
static const osw_field_desc fields_199[] = {
    {"command", 16u, 13u, 0u, 0u},
    {"topology", 56u, 3u, 1u, 0u},
};
static const osw_field_desc fields_202[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"object", 56u, 13u, 1u, 0u},
    {"allocator_pointer", 96u, 4u, 2u, 0u},
};
static const osw_field_desc fields_205[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"object", 56u, 13u, 1u, 0u},
    {"allocator_pointer", 96u, 4u, 2u, 0u},
};
static const osw_field_desc fields_208[] = {
    {"device", 16u, 13u, 0u, 0u},
    {"object", 56u, 13u, 1u, 0u},
    {"allocator_pointer", 96u, 4u, 2u, 0u},
};
static const osw_field_desc fields_211[] = {
    {"code", 16u, 1u, 0u, 0u},
};
static const osw_field_desc fields_214[] = {
    {"vk_command_buffer", 16u, 4u, 0u, 0u},
    {"viewports", 24u, 51u, 1u, 0u},
};
static const osw_field_desc fields_217[] = {
    {"vk_command_buffer", 16u, 4u, 0u, 0u},
    {"rects", 24u, 79u, 1u, 0u},
};
static const osw_field_desc fields_220[] = {
    {"command_buffer", 16u, 4u, 0u, 1u},
    {"pso_pointer", 24u, 4u, 1u, 1u},
    {"device_interface_pointer", 32u, 4u, 2u, 1u},
    {"device_origin", 40u, 1u, 3u, 1u},
    {"root_signature_pointer", 48u, 4u, 4u, 1u},
    {"pso_root_pointer", 56u, 4u, 5u, 1u},
};
static const osw_field_desc fields_221[] = {
    {"raw_receiver", 16u, 4u, 0u, 1u},
    {"command_interface_pointer", 24u, 4u, 1u, 1u},
    {"pso_internal_pointer", 32u, 4u, 2u, 1u},
    {"root_internal_pointer", 40u, 4u, 3u, 1u},
    {"source_site", 48u, 1u, 4u, 1u},
    {"pipeline_route", 52u, 1u, 5u, 1u},
};
static const osw_field_desc fields_270[] = {
    {"id", 16u, 5u, 0u, 0u},
    {"entry", 24u, 5u, 1u, 0u},
    {"return", 32u, 5u, 2u, 1u},
    {"parent", 40u, 5u, 3u, 1u},
    {"api", 48u, 1u, 4u, 0u},
    {"domain", 52u, 1u, 5u, 0u},
    {"source_before", 56u, 15u, 6u, 0u},
    {"source_after", 232u, 17u, 7u, 0u},
    {"args", 432u, 268u, 8u, 0u},
    {"result", 1872u, 269u, 9u, 1u},
};
static const osw_type_desc type_desc[] = {
    {"U16", T_U16, 2u, 0u, 0u, 0u, NULL},
    {"U32", T_U32, 4u, 0u, 0u, 0u, NULL},
    {"I32", T_I32, 4u, 0u, 0u, 0u, NULL},
    {"Enum32", T_I32, 4u, 0u, 0u, 0u, NULL},
    {"U64", T_U64, 8u, 0u, 0u, 0u, NULL},
    {"ID", T_U64, 8u, 0u, 0u, 0u, NULL},
    {"Bool", T_BOOL, 4u, 0u, 0u, 0u, NULL},
    {"String", T_STRING, 260u, 0u, 0u, 0u, NULL},
    {"Nonce32", T_NONCE, 32u, 0u, 0u, 0u, NULL},
    {"SHA256", T_SHA, 32u, 0u, 0u, 0u, NULL},
    {"GUID", T_GUID, 16u, 0u, 0u, 0u, NULL},
    {"Code", T_OBJECT, 72u, 0u, 0u, 4u, fields_11},
    {"Ref", T_OBJECT, 40u, 0u, 0u, 3u, fields_12},
    {"ObjectArg", T_OBJECT, 40u, 0u, 0u, 3u, fields_13},
    {"Birth", T_OBJECT, 112u, 0u, 0u, 4u, fields_14},
    {"SourceScope", T_OBJECT, 176u, 0u, 0u, 8u, fields_15},
    {"Array_Birth_1", T_ARRAY, 120u, 14u, 1u, 0u, NULL},
    {"SourceAfter", T_OBJECT, 200u, 0u, 0u, 9u, fields_17},
    {"ResourceDesc", T_OBJECT, 72u, 0u, 0u, 11u, fields_18},
    {"HeapDesc", T_OBJECT, 32u, 0u, 0u, 4u, fields_19},
    {"SRV", T_OBJECT, 48u, 0u, 0u, 7u, fields_20},
    {"RootRange", T_OBJECT, 40u, 0u, 0u, 6u, fields_21},
    {"RootConstants", T_OBJECT, 32u, 0u, 0u, 3u, fields_22},
    {"RootDescriptor", T_OBJECT, 32u, 0u, 0u, 3u, fields_23},
    {"Array_RootRange_1", T_ARRAY, 48u, 21u, 1u, 0u, NULL},
    {"SpanRootRanges", T_OBJECT, 88u, 0u, 0u, 4u, fields_25},
    {"RootParameter", T_OBJECT, 176u, 0u, 0u, 5u, fields_26},
    {"Array_RootParameter_3", T_ARRAY, 536u, 26u, 3u, 0u, NULL},
    {"SpanRootParameters", T_OBJECT, 576u, 0u, 0u, 4u, fields_28},
    {"RootDesc", T_OBJECT, 608u, 0u, 0u, 4u, fields_29},
    {"Viewport", T_OBJECT, 40u, 0u, 0u, 6u, fields_30},
    {"Rect", T_OBJECT, 32u, 0u, 0u, 4u, fields_31},
    {"Array_U64_3", T_ARRAY, 32u, 4u, 3u, 0u, NULL},
    {"Array_U32_3", T_ARRAY, 20u, 1u, 3u, 0u, NULL},
    {"RangeSide", T_OBJECT, 96u, 0u, 0u, 5u, fields_34},
    {"Stage", T_OBJECT, 360u, 0u, 0u, 8u, fields_35},
    {"Capture", T_OBJECT, 128u, 0u, 0u, 13u, fields_36},
    {"Ticket", T_OBJECT, 40u, 0u, 0u, 3u, fields_37},
    {"Array_ID_8", T_ARRAY, 72u, 5u, 8u, 0u, NULL},
    {"Diagnostic", T_OBJECT, 352u, 0u, 0u, 2u, fields_39},
    {"LogicalSlot", T_OBJECT, 72u, 0u, 0u, 3u, fields_40},
    {"ViewResult", T_OBJECT, 264u, 0u, 0u, 14u, fields_41},
    {"ScissorObservation", T_OBJECT, 56u, 0u, 0u, 2u, fields_42},
    {"ViewportObservation", T_OBJECT, 64u, 0u, 0u, 2u, fields_43},
    {"Array_U64_8", T_ARRAY, 72u, 4u, 8u, 0u, NULL},
    {"SpanSetLayouts", T_OBJECT, 112u, 0u, 0u, 4u, fields_45},
    {"Array_Stage_2", T_ARRAY, 728u, 35u, 2u, 0u, NULL},
    {"SpanStages", T_OBJECT, 768u, 0u, 0u, 4u, fields_47},
    {"Array_ObjectArg_2", T_ARRAY, 88u, 13u, 2u, 0u, NULL},
    {"SpanHeaps", T_OBJECT, 128u, 0u, 0u, 4u, fields_49},
    {"Array_Viewport_1", T_ARRAY, 48u, 30u, 1u, 0u, NULL},
    {"SpanViewports", T_OBJECT, 88u, 0u, 0u, 4u, fields_51},
    {"Array_Rect_1", T_ARRAY, 40u, 31u, 1u, 0u, NULL},
    {"SpanRects", T_OBJECT, 80u, 0u, 0u, 4u, fields_53},
    {"Array_U64_1", T_ARRAY, 16u, 4u, 1u, 0u, NULL},
    {"SpanRTVHandles", T_OBJECT, 56u, 0u, 0u, 4u, fields_55},
    {"Array_ObjectArg_1", T_ARRAY, 48u, 13u, 1u, 0u, NULL},
    {"SpanCommands", T_OBJECT, 88u, 0u, 0u, 4u, fields_57},
    {"HeapProperties", T_OBJECT, 40u, 0u, 0u, 5u, fields_58},
    {"Array_U32_2", T_ARRAY, 16u, 1u, 2u, 0u, NULL},
    {"SpanU32Words", T_OBJECT, 56u, 0u, 0u, 4u, fields_60},
    {"Array_Enum32_1", T_ARRAY, 12u, 3u, 1u, 0u, NULL},
    {"SpanFormats", T_OBJECT, 48u, 0u, 0u, 4u, fields_62},
    {"Array_U32_4", T_ARRAY, 24u, 1u, 4u, 0u, NULL},
    {"SamplerDesc", T_OBJECT, 80u, 0u, 0u, 10u, fields_64},
    {"PushRange", T_OBJECT, 32u, 0u, 0u, 3u, fields_65},
    {"Array_PushRange_1", T_ARRAY, 40u, 65u, 1u, 0u, NULL},
    {"SpanPushRanges", T_OBJECT, 80u, 0u, 0u, 4u, fields_67},
    {"RenderingPayload", T_OBJECT, 80u, 0u, 0u, 4u, fields_68},
    {"Flags2Payload", T_OBJECT, 24u, 0u, 0u, 1u, fields_69},
    {"NextPayloadUnion", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"PipelineNextNode", T_OBJECT, 128u, 0u, 0u, 5u, fields_71},
    {"Array_PipelineNextNode_2", T_ARRAY, 264u, 71u, 2u, 0u, NULL},
    {"PipelineNext", T_OBJECT, 296u, 0u, 0u, 3u, fields_73},
    {"SpanPipelineLibraries", T_OBJECT, 72u, 0u, 0u, 4u, fields_74},
    {"LibraryProjection", T_OBJECT, 104u, 0u, 0u, 3u, fields_75},
    {"OwnedAuxResult", T_OBJECT, 200u, 0u, 0u, 10u, fields_76},
    {"VkRect", T_OBJECT, 32u, 0u, 0u, 4u, fields_77},
    {"Array_VkRect_1", T_ARRAY, 40u, 77u, 1u, 0u, NULL},
    {"SpanVkRects", T_OBJECT, 80u, 0u, 0u, 4u, fields_79},
    {"VkRasterResult", T_OBJECT, 104u, 0u, 0u, 4u, fields_80},
    {"PipelineStatePointers", T_OBJECT, 88u, 0u, 0u, 9u, fields_81},
    {"Args_DeviceCreate", T_OBJECT, 24u, 0u, 0u, 1u, fields_82},
    {"Result_DeviceCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_83},
    {"Before_DeviceCreate", T_UNION, 32u, 0u, 0u, 0u, NULL},
    {"Args_VkDeviceCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_85},
    {"Result_VkDeviceCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_86},
    {"Before_VkDeviceCreate", T_UNION, 40u, 0u, 0u, 0u, NULL},
    {"Args_ResourceCreate", T_OBJECT, 232u, 0u, 0u, 10u, fields_88},
    {"Result_ResourceCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_89},
    {"Before_ResourceCreate", T_UNION, 240u, 0u, 0u, 0u, NULL},
    {"Args_HeapCreate", T_OBJECT, 120u, 0u, 0u, 5u, fields_91},
    {"Result_HeapCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_92},
    {"Before_HeapCreate", T_UNION, 128u, 0u, 0u, 0u, NULL},
    {"Args_AllocatorCreate", T_OBJECT, 72u, 0u, 0u, 3u, fields_94},
    {"Result_AllocatorCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_95},
    {"Before_AllocatorCreate", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"Args_QueueCreate", T_OBJECT, 72u, 0u, 0u, 3u, fields_97},
    {"Result_QueueCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_98},
    {"Before_QueueCreate", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"Args_CommandCreate", T_OBJECT, 168u, 0u, 0u, 7u, fields_100},
    {"Result_CommandCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_101},
    {"Before_CommandCreate", T_UNION, 176u, 0u, 0u, 0u, NULL},
    {"Args_RootCreate", T_OBJECT, 144u, 0u, 0u, 4u, fields_103},
    {"Result_RootCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_104},
    {"Before_RootCreate", T_UNION, 152u, 0u, 0u, 0u, NULL},
    {"Args_RootParse", T_OBJECT, 96u, 0u, 0u, 2u, fields_106},
    {"Result_RootParse", T_OBJECT, 632u, 0u, 0u, 2u, fields_107},
    {"Before_RootParse", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"Args_PSOCreate", T_OBJECT, 304u, 0u, 0u, 8u, fields_109},
    {"Result_PSOCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_110},
    {"Before_PSOCreate", T_UNION, 312u, 0u, 0u, 0u, NULL},
    {"Args_ModuleCreate", T_OBJECT, 176u, 0u, 0u, 8u, fields_112},
    {"Result_ModuleCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_113},
    {"Before_ModuleCreate", T_UNION, 184u, 0u, 0u, 0u, NULL},
    {"Args_PipelineLayoutCreate", T_OBJECT, 296u, 0u, 0u, 9u, fields_115},
    {"Result_PipelineLayoutCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_116},
    {"Before_PipelineLayoutCreate", T_UNION, 304u, 0u, 0u, 0u, NULL},
    {"Args_PipelineCreate", T_OBJECT, 1440u, 0u, 0u, 18u, fields_118},
    {"Result_PipelineCreate", T_OBJECT, 32u, 0u, 0u, 2u, fields_119},
    {"Before_PipelineCreate", T_UNION, 1448u, 0u, 0u, 0u, NULL},
    {"Args_Increment", T_OBJECT, 64u, 0u, 0u, 2u, fields_121},
    {"Result_Increment", T_OBJECT, 24u, 0u, 0u, 1u, fields_122},
    {"Before_Increment", T_UNION, 72u, 0u, 0u, 0u, NULL},
    {"Args_CPUStart", T_OBJECT, 64u, 0u, 0u, 2u, fields_124},
    {"Result_CPUStart", T_OBJECT, 32u, 0u, 0u, 2u, fields_125},
    {"Before_CPUStart", T_UNION, 72u, 0u, 0u, 0u, NULL},
    {"Args_GPUStart", T_OBJECT, 64u, 0u, 0u, 2u, fields_127},
    {"Result_GPUStart", T_OBJECT, 32u, 0u, 0u, 2u, fields_128},
    {"Before_GPUStart", T_UNION, 72u, 0u, 0u, 0u, NULL},
    {"Args_CreateSRV", T_OBJECT, 160u, 0u, 0u, 5u, fields_130},
    {"Result_CreateSRV", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_CreateSRV", T_UNION, 168u, 0u, 0u, 0u, NULL},
    {"Args_CopySimple", T_OBJECT, 88u, 0u, 0u, 5u, fields_133},
    {"Result_CopySimple", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_CopySimple", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"Args_CopyRanges", T_OBJECT, 256u, 0u, 0u, 4u, fields_136},
    {"Result_CopyRanges", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_CopyRanges", T_UNION, 264u, 0u, 0u, 0u, NULL},
    {"Args_Reset", T_OBJECT, 136u, 0u, 0u, 3u, fields_139},
    {"Result_Reset", T_OBJECT, 24u, 0u, 0u, 1u, fields_140},
    {"Before_Reset", T_UNION, 144u, 0u, 0u, 0u, NULL},
    {"Args_Close", T_OBJECT, 56u, 0u, 0u, 1u, fields_142},
    {"Result_Close", T_OBJECT, 24u, 0u, 0u, 1u, fields_143},
    {"Before_Close", T_UNION, 64u, 0u, 0u, 0u, NULL},
    {"Args_ClearState", T_OBJECT, 96u, 0u, 0u, 2u, fields_145},
    {"Result_ClearState", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_ClearState", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"Args_SetPipeline", T_OBJECT, 96u, 0u, 0u, 2u, fields_148},
    {"Result_SetPipeline", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_SetPipeline", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"Args_SetRoot", T_OBJECT, 96u, 0u, 0u, 2u, fields_151},
    {"Result_SetRoot", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_SetRoot", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"Args_SetHeaps", T_OBJECT, 184u, 0u, 0u, 2u, fields_154},
    {"Result_SetHeaps", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_SetHeaps", T_UNION, 192u, 0u, 0u, 0u, NULL},
    {"Args_SetTable", T_OBJECT, 72u, 0u, 0u, 3u, fields_157},
    {"Result_SetTable", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_SetTable", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"Args_Viewports", T_OBJECT, 144u, 0u, 0u, 2u, fields_160},
    {"Result_Viewports", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_Viewports", T_UNION, 152u, 0u, 0u, 0u, NULL},
    {"Args_Scissors", T_OBJECT, 136u, 0u, 0u, 2u, fields_163},
    {"Result_Scissors", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_Scissors", T_UNION, 144u, 0u, 0u, 0u, NULL},
    {"Args_RTV", T_OBJECT, 128u, 0u, 0u, 4u, fields_166},
    {"Result_RTV", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_RTV", T_UNION, 136u, 0u, 0u, 0u, NULL},
    {"Args_Draw", T_OBJECT, 72u, 0u, 0u, 5u, fields_169},
    {"Result_Draw", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_Draw", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"Args_BindPipeline", T_OBJECT, 72u, 0u, 0u, 3u, fields_172},
    {"Result_BindPipeline", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_BindPipeline", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"Args_EmitDraw", T_OBJECT, 40u, 0u, 0u, 5u, fields_175},
    {"Result_EmitDraw", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_EmitDraw", T_UNION, 48u, 0u, 0u, 0u, NULL},
    {"Args_Execute", T_OBJECT, 144u, 0u, 0u, 2u, fields_178},
    {"Result_Execute", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_Execute", T_UNION, 152u, 0u, 0u, 0u, NULL},
    {"Args_Release", T_OBJECT, 56u, 0u, 0u, 1u, fields_181},
    {"Result_Release", T_OBJECT, 24u, 0u, 0u, 1u, fields_182},
    {"Before_Release", T_UNION, 64u, 0u, 0u, 0u, NULL},
    {"Args_AllocatorReset", T_OBJECT, 56u, 0u, 0u, 1u, fields_184},
    {"Result_AllocatorReset", T_OBJECT, 24u, 0u, 0u, 1u, fields_185},
    {"Before_AllocatorReset", T_UNION, 64u, 0u, 0u, 0u, NULL},
    {"Args_PipelineLookup", T_OBJECT, 48u, 0u, 0u, 4u, fields_187},
    {"Result_PipelineLookup", T_OBJECT, 32u, 0u, 0u, 2u, fields_188},
    {"Before_PipelineLookup", T_UNION, 56u, 0u, 0u, 0u, NULL},
    {"Args_CreateSampler", T_OBJECT, 160u, 0u, 0u, 5u, fields_190},
    {"Result_CreateSampler", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_CreateSampler", T_UNION, 168u, 0u, 0u, 0u, NULL},
    {"Args_CreateRTV", T_OBJECT, 112u, 0u, 0u, 4u, fields_193},
    {"Result_CreateRTV", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_CreateRTV", T_UNION, 120u, 0u, 0u, 0u, NULL},
    {"Args_SetConstants", T_OBJECT, 128u, 0u, 0u, 4u, fields_196},
    {"Result_SetConstants", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_SetConstants", T_UNION, 136u, 0u, 0u, 0u, NULL},
    {"Args_Topology", T_OBJECT, 64u, 0u, 0u, 2u, fields_199},
    {"Result_Topology", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_Topology", T_UNION, 72u, 0u, 0u, 0u, NULL},
    {"Args_ModuleDestroy", T_OBJECT, 104u, 0u, 0u, 3u, fields_202},
    {"Result_ModuleDestroy", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_ModuleDestroy", T_UNION, 112u, 0u, 0u, 0u, NULL},
    {"Args_PipelineDestroy", T_OBJECT, 104u, 0u, 0u, 3u, fields_205},
    {"Result_PipelineDestroy", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_PipelineDestroy", T_UNION, 112u, 0u, 0u, 0u, NULL},
    {"Args_PipelineLayoutDestroy", T_OBJECT, 104u, 0u, 0u, 3u, fields_208},
    {"Result_PipelineLayoutDestroy", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_PipelineLayoutDestroy", T_UNION, 112u, 0u, 0u, 0u, NULL},
    {"Args_ScopeGap", T_OBJECT, 24u, 0u, 0u, 1u, fields_211},
    {"Result_ScopeGap", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_ScopeGap", T_UNION, 32u, 0u, 0u, 0u, NULL},
    {"Args_VkViewportCount", T_OBJECT, 112u, 0u, 0u, 2u, fields_214},
    {"Result_VkViewportCount", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_VkViewportCount", T_UNION, 120u, 0u, 0u, 0u, NULL},
    {"Args_VkScissorCount", T_OBJECT, 104u, 0u, 0u, 2u, fields_217},
    {"Result_VkScissorCount", T_UNION, 1u, 0u, 0u, 0u, NULL},
    {"Before_VkScissorCount", T_UNION, 112u, 0u, 0u, 0u, NULL},
    {"SourceAfterInput", T_OBJECT, 64u, 0u, 0u, 6u, fields_220},
    {"SourceEntryInput", T_OBJECT, 56u, 0u, 0u, 6u, fields_221},
    {"After_DeviceCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_VkDeviceCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_ResourceCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_HeapCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_AllocatorCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_QueueCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_CommandCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_RootCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_RootParse", T_UNION, 704u, 0u, 0u, 0u, NULL},
    {"After_PSOCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_ModuleCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_PipelineLayoutCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_PipelineCreate", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_Increment", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"After_CPUStart", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_GPUStart", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_CreateSRV", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_CopySimple", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_CopyRanges", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Reset", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"After_Close", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"After_ClearState", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_SetPipeline", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_SetRoot", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_SetHeaps", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_SetTable", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Viewports", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Scissors", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_RTV", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Draw", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_BindPipeline", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_EmitDraw", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Execute", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Release", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"After_AllocatorReset", T_UNION, 96u, 0u, 0u, 0u, NULL},
    {"After_PipelineLookup", T_UNION, 104u, 0u, 0u, 0u, NULL},
    {"After_CreateSampler", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_CreateRTV", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_SetConstants", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_Topology", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_ModuleDestroy", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_PipelineDestroy", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_PipelineLayoutDestroy", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_ScopeGap", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_VkViewportCount", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"After_VkScissorCount", T_UNION, 80u, 0u, 0u, 0u, NULL},
    {"ArgsUnion", T_UNION, 1440u, 0u, 0u, 0u, NULL},
    {"ResultsUnion", T_UNION, 632u, 0u, 0u, 0u, NULL},
    {"Call", T_OBJECT, 2504u, 0u, 0u, 10u, fields_270},
    {"Token", T_UNION, 48u, 0u, 0u, 0u, NULL},
    {"Admission", T_UNION, 24u, 0u, 0u, 0u, NULL},
    {"AliasSlot", T_UNION, 32u, 0u, 0u, 0u, NULL},
};
#define TYPE_U16 0u
#define TYPE_U32 1u
#define TYPE_I32 2u
#define TYPE_ENUM32 3u
#define TYPE_U64 4u
#define TYPE_ID 5u
#define TYPE_BOOL 6u
#define TYPE_STRING 7u
#define TYPE_NONCE32 8u
#define TYPE_SHA256 9u
#define TYPE_GUID 10u
#define TYPE_CODE 11u
#define TYPE_REF 12u
#define TYPE_OBJECTARG 13u
#define TYPE_BIRTH 14u
#define TYPE_SOURCESCOPE 15u
#define TYPE_ARRAY_BIRTH_1 16u
#define TYPE_SOURCEAFTER 17u
#define TYPE_RESOURCEDESC 18u
#define TYPE_HEAPDESC 19u
#define TYPE_SRV 20u
#define TYPE_ROOTRANGE 21u
#define TYPE_ROOTCONSTANTS 22u
#define TYPE_ROOTDESCRIPTOR 23u
#define TYPE_ARRAY_ROOTRANGE_1 24u
#define TYPE_SPANROOTRANGES 25u
#define TYPE_ROOTPARAMETER 26u
#define TYPE_ARRAY_ROOTPARAMETER_3 27u
#define TYPE_SPANROOTPARAMETERS 28u
#define TYPE_ROOTDESC 29u
#define TYPE_VIEWPORT 30u
#define TYPE_RECT 31u
#define TYPE_ARRAY_U64_3 32u
#define TYPE_ARRAY_U32_3 33u
#define TYPE_RANGESIDE 34u
#define TYPE_STAGE 35u
#define TYPE_CAPTURE 36u
#define TYPE_TICKET 37u
#define TYPE_ARRAY_ID_8 38u
#define TYPE_DIAGNOSTIC 39u
#define TYPE_LOGICALSLOT 40u
#define TYPE_VIEWRESULT 41u
#define TYPE_SCISSOROBSERVATION 42u
#define TYPE_VIEWPORTOBSERVATION 43u
#define TYPE_ARRAY_U64_8 44u
#define TYPE_SPANSETLAYOUTS 45u
#define TYPE_ARRAY_STAGE_2 46u
#define TYPE_SPANSTAGES 47u
#define TYPE_ARRAY_OBJECTARG_2 48u
#define TYPE_SPANHEAPS 49u
#define TYPE_ARRAY_VIEWPORT_1 50u
#define TYPE_SPANVIEWPORTS 51u
#define TYPE_ARRAY_RECT_1 52u
#define TYPE_SPANRECTS 53u
#define TYPE_ARRAY_U64_1 54u
#define TYPE_SPANRTVHANDLES 55u
#define TYPE_ARRAY_OBJECTARG_1 56u
#define TYPE_SPANCOMMANDS 57u
#define TYPE_HEAPPROPERTIES 58u
#define TYPE_ARRAY_U32_2 59u
#define TYPE_SPANU32WORDS 60u
#define TYPE_ARRAY_ENUM32_1 61u
#define TYPE_SPANFORMATS 62u
#define TYPE_ARRAY_U32_4 63u
#define TYPE_SAMPLERDESC 64u
#define TYPE_PUSHRANGE 65u
#define TYPE_ARRAY_PUSHRANGE_1 66u
#define TYPE_SPANPUSHRANGES 67u
#define TYPE_RENDERINGPAYLOAD 68u
#define TYPE_FLAGS2PAYLOAD 69u
#define TYPE_NEXTPAYLOADUNION 70u
#define TYPE_PIPELINENEXTNODE 71u
#define TYPE_ARRAY_PIPELINENEXTNODE_2 72u
#define TYPE_PIPELINENEXT 73u
#define TYPE_SPANPIPELINELIBRARIES 74u
#define TYPE_LIBRARYPROJECTION 75u
#define TYPE_OWNEDAUXRESULT 76u
#define TYPE_VKRECT 77u
#define TYPE_ARRAY_VKRECT_1 78u
#define TYPE_SPANVKRECTS 79u
#define TYPE_VKRASTERRESULT 80u
#define TYPE_PIPELINESTATEPOINTERS 81u
#define TYPE_ARGS_DEVICECREATE 82u
#define TYPE_RESULT_DEVICECREATE 83u
#define TYPE_BEFORE_DEVICECREATE 84u
#define TYPE_ARGS_VKDEVICECREATE 85u
#define TYPE_RESULT_VKDEVICECREATE 86u
#define TYPE_BEFORE_VKDEVICECREATE 87u
#define TYPE_ARGS_RESOURCECREATE 88u
#define TYPE_RESULT_RESOURCECREATE 89u
#define TYPE_BEFORE_RESOURCECREATE 90u
#define TYPE_ARGS_HEAPCREATE 91u
#define TYPE_RESULT_HEAPCREATE 92u
#define TYPE_BEFORE_HEAPCREATE 93u
#define TYPE_ARGS_ALLOCATORCREATE 94u
#define TYPE_RESULT_ALLOCATORCREATE 95u
#define TYPE_BEFORE_ALLOCATORCREATE 96u
#define TYPE_ARGS_QUEUECREATE 97u
#define TYPE_RESULT_QUEUECREATE 98u
#define TYPE_BEFORE_QUEUECREATE 99u
#define TYPE_ARGS_COMMANDCREATE 100u
#define TYPE_RESULT_COMMANDCREATE 101u
#define TYPE_BEFORE_COMMANDCREATE 102u
#define TYPE_ARGS_ROOTCREATE 103u
#define TYPE_RESULT_ROOTCREATE 104u
#define TYPE_BEFORE_ROOTCREATE 105u
#define TYPE_ARGS_ROOTPARSE 106u
#define TYPE_RESULT_ROOTPARSE 107u
#define TYPE_BEFORE_ROOTPARSE 108u
#define TYPE_ARGS_PSOCREATE 109u
#define TYPE_RESULT_PSOCREATE 110u
#define TYPE_BEFORE_PSOCREATE 111u
#define TYPE_ARGS_MODULECREATE 112u
#define TYPE_RESULT_MODULECREATE 113u
#define TYPE_BEFORE_MODULECREATE 114u
#define TYPE_ARGS_PIPELINELAYOUTCREATE 115u
#define TYPE_RESULT_PIPELINELAYOUTCREATE 116u
#define TYPE_BEFORE_PIPELINELAYOUTCREATE 117u
#define TYPE_ARGS_PIPELINECREATE 118u
#define TYPE_RESULT_PIPELINECREATE 119u
#define TYPE_BEFORE_PIPELINECREATE 120u
#define TYPE_ARGS_INCREMENT 121u
#define TYPE_RESULT_INCREMENT 122u
#define TYPE_BEFORE_INCREMENT 123u
#define TYPE_ARGS_CPUSTART 124u
#define TYPE_RESULT_CPUSTART 125u
#define TYPE_BEFORE_CPUSTART 126u
#define TYPE_ARGS_GPUSTART 127u
#define TYPE_RESULT_GPUSTART 128u
#define TYPE_BEFORE_GPUSTART 129u
#define TYPE_ARGS_CREATESRV 130u
#define TYPE_RESULT_CREATESRV 131u
#define TYPE_BEFORE_CREATESRV 132u
#define TYPE_ARGS_COPYSIMPLE 133u
#define TYPE_RESULT_COPYSIMPLE 134u
#define TYPE_BEFORE_COPYSIMPLE 135u
#define TYPE_ARGS_COPYRANGES 136u
#define TYPE_RESULT_COPYRANGES 137u
#define TYPE_BEFORE_COPYRANGES 138u
#define TYPE_ARGS_RESET 139u
#define TYPE_RESULT_RESET 140u
#define TYPE_BEFORE_RESET 141u
#define TYPE_ARGS_CLOSE 142u
#define TYPE_RESULT_CLOSE 143u
#define TYPE_BEFORE_CLOSE 144u
#define TYPE_ARGS_CLEARSTATE 145u
#define TYPE_RESULT_CLEARSTATE 146u
#define TYPE_BEFORE_CLEARSTATE 147u
#define TYPE_ARGS_SETPIPELINE 148u
#define TYPE_RESULT_SETPIPELINE 149u
#define TYPE_BEFORE_SETPIPELINE 150u
#define TYPE_ARGS_SETROOT 151u
#define TYPE_RESULT_SETROOT 152u
#define TYPE_BEFORE_SETROOT 153u
#define TYPE_ARGS_SETHEAPS 154u
#define TYPE_RESULT_SETHEAPS 155u
#define TYPE_BEFORE_SETHEAPS 156u
#define TYPE_ARGS_SETTABLE 157u
#define TYPE_RESULT_SETTABLE 158u
#define TYPE_BEFORE_SETTABLE 159u
#define TYPE_ARGS_VIEWPORTS 160u
#define TYPE_RESULT_VIEWPORTS 161u
#define TYPE_BEFORE_VIEWPORTS 162u
#define TYPE_ARGS_SCISSORS 163u
#define TYPE_RESULT_SCISSORS 164u
#define TYPE_BEFORE_SCISSORS 165u
#define TYPE_ARGS_RTV 166u
#define TYPE_RESULT_RTV 167u
#define TYPE_BEFORE_RTV 168u
#define TYPE_ARGS_DRAW 169u
#define TYPE_RESULT_DRAW 170u
#define TYPE_BEFORE_DRAW 171u
#define TYPE_ARGS_BINDPIPELINE 172u
#define TYPE_RESULT_BINDPIPELINE 173u
#define TYPE_BEFORE_BINDPIPELINE 174u
#define TYPE_ARGS_EMITDRAW 175u
#define TYPE_RESULT_EMITDRAW 176u
#define TYPE_BEFORE_EMITDRAW 177u
#define TYPE_ARGS_EXECUTE 178u
#define TYPE_RESULT_EXECUTE 179u
#define TYPE_BEFORE_EXECUTE 180u
#define TYPE_ARGS_RELEASE 181u
#define TYPE_RESULT_RELEASE 182u
#define TYPE_BEFORE_RELEASE 183u
#define TYPE_ARGS_ALLOCATORRESET 184u
#define TYPE_RESULT_ALLOCATORRESET 185u
#define TYPE_BEFORE_ALLOCATORRESET 186u
#define TYPE_ARGS_PIPELINELOOKUP 187u
#define TYPE_RESULT_PIPELINELOOKUP 188u
#define TYPE_BEFORE_PIPELINELOOKUP 189u
#define TYPE_ARGS_CREATESAMPLER 190u
#define TYPE_RESULT_CREATESAMPLER 191u
#define TYPE_BEFORE_CREATESAMPLER 192u
#define TYPE_ARGS_CREATERTV 193u
#define TYPE_RESULT_CREATERTV 194u
#define TYPE_BEFORE_CREATERTV 195u
#define TYPE_ARGS_SETCONSTANTS 196u
#define TYPE_RESULT_SETCONSTANTS 197u
#define TYPE_BEFORE_SETCONSTANTS 198u
#define TYPE_ARGS_TOPOLOGY 199u
#define TYPE_RESULT_TOPOLOGY 200u
#define TYPE_BEFORE_TOPOLOGY 201u
#define TYPE_ARGS_MODULEDESTROY 202u
#define TYPE_RESULT_MODULEDESTROY 203u
#define TYPE_BEFORE_MODULEDESTROY 204u
#define TYPE_ARGS_PIPELINEDESTROY 205u
#define TYPE_RESULT_PIPELINEDESTROY 206u
#define TYPE_BEFORE_PIPELINEDESTROY 207u
#define TYPE_ARGS_PIPELINELAYOUTDESTROY 208u
#define TYPE_RESULT_PIPELINELAYOUTDESTROY 209u
#define TYPE_BEFORE_PIPELINELAYOUTDESTROY 210u
#define TYPE_ARGS_SCOPEGAP 211u
#define TYPE_RESULT_SCOPEGAP 212u
#define TYPE_BEFORE_SCOPEGAP 213u
#define TYPE_ARGS_VKVIEWPORTCOUNT 214u
#define TYPE_RESULT_VKVIEWPORTCOUNT 215u
#define TYPE_BEFORE_VKVIEWPORTCOUNT 216u
#define TYPE_ARGS_VKSCISSORCOUNT 217u
#define TYPE_RESULT_VKSCISSORCOUNT 218u
#define TYPE_BEFORE_VKSCISSORCOUNT 219u
#define TYPE_SOURCEAFTERINPUT 220u
#define TYPE_SOURCEENTRYINPUT 221u
#define TYPE_AFTER_DEVICECREATE 222u
#define TYPE_AFTER_VKDEVICECREATE 223u
#define TYPE_AFTER_RESOURCECREATE 224u
#define TYPE_AFTER_HEAPCREATE 225u
#define TYPE_AFTER_ALLOCATORCREATE 226u
#define TYPE_AFTER_QUEUECREATE 227u
#define TYPE_AFTER_COMMANDCREATE 228u
#define TYPE_AFTER_ROOTCREATE 229u
#define TYPE_AFTER_ROOTPARSE 230u
#define TYPE_AFTER_PSOCREATE 231u
#define TYPE_AFTER_MODULECREATE 232u
#define TYPE_AFTER_PIPELINELAYOUTCREATE 233u
#define TYPE_AFTER_PIPELINECREATE 234u
#define TYPE_AFTER_INCREMENT 235u
#define TYPE_AFTER_CPUSTART 236u
#define TYPE_AFTER_GPUSTART 237u
#define TYPE_AFTER_CREATESRV 238u
#define TYPE_AFTER_COPYSIMPLE 239u
#define TYPE_AFTER_COPYRANGES 240u
#define TYPE_AFTER_RESET 241u
#define TYPE_AFTER_CLOSE 242u
#define TYPE_AFTER_CLEARSTATE 243u
#define TYPE_AFTER_SETPIPELINE 244u
#define TYPE_AFTER_SETROOT 245u
#define TYPE_AFTER_SETHEAPS 246u
#define TYPE_AFTER_SETTABLE 247u
#define TYPE_AFTER_VIEWPORTS 248u
#define TYPE_AFTER_SCISSORS 249u
#define TYPE_AFTER_RTV 250u
#define TYPE_AFTER_DRAW 251u
#define TYPE_AFTER_BINDPIPELINE 252u
#define TYPE_AFTER_EMITDRAW 253u
#define TYPE_AFTER_EXECUTE 254u
#define TYPE_AFTER_RELEASE 255u
#define TYPE_AFTER_ALLOCATORRESET 256u
#define TYPE_AFTER_PIPELINELOOKUP 257u
#define TYPE_AFTER_CREATESAMPLER 258u
#define TYPE_AFTER_CREATERTV 259u
#define TYPE_AFTER_SETCONSTANTS 260u
#define TYPE_AFTER_TOPOLOGY 261u
#define TYPE_AFTER_MODULEDESTROY 262u
#define TYPE_AFTER_PIPELINEDESTROY 263u
#define TYPE_AFTER_PIPELINELAYOUTDESTROY 264u
#define TYPE_AFTER_SCOPEGAP 265u
#define TYPE_AFTER_VKVIEWPORTCOUNT 266u
#define TYPE_AFTER_VKSCISSORCOUNT 267u
#define TYPE_ARGSUNION 268u
#define TYPE_RESULTSUNION 269u
#define TYPE_CALL 270u
#define TYPE_TOKEN 271u
#define TYPE_ADMISSION 272u
#define TYPE_ALIASSLOT 273u
static const osw_event_desc event_desc[47] = { {0},
    {"DeviceCreate", "source_original", 82u, 83u, 32u, 104u, 40u, 0u},
    {"VkDeviceCreate", "Vulkan_original", 85u, 86u, 40u, 104u, 40u, 0u},
    {"ResourceCreate", "COM_original", 88u, 89u, 240u, 104u, 40u, 0u},
    {"HeapCreate", "COM_original", 91u, 92u, 128u, 104u, 40u, 0u},
    {"AllocatorCreate", "COM_original", 94u, 95u, 80u, 104u, 40u, 0u},
    {"QueueCreate", "COM_original", 97u, 98u, 80u, 104u, 40u, 0u},
    {"CommandCreate", "COM_original", 100u, 101u, 176u, 104u, 40u, 0u},
    {"RootCreate", "COM_original", 103u, 104u, 152u, 104u, 40u, 0u},
    {"RootParse", "source_original", 106u, 107u, 104u, 704u, 640u, 0u},
    {"PSOCreate", "COM_original", 109u, 110u, 312u, 104u, 40u, 0u},
    {"ModuleCreate", "Vulkan_original", 112u, 113u, 184u, 104u, 40u, 0u},
    {"PipelineLayoutCreate", "Vulkan_original", 115u, 116u, 304u, 104u, 40u, 0u},
    {"PipelineCreate", "Vulkan_original", 118u, 119u, 1448u, 104u, 40u, 0u},
    {"Increment", "COM_original", 121u, 122u, 72u, 96u, 32u, 0u},
    {"CPUStart", "source_original", 124u, 125u, 72u, 104u, 40u, 0u},
    {"GPUStart", "source_original", 127u, 128u, 72u, 104u, 40u, 0u},
    {"CreateSRV", "COM_original", 130u, 131u, 168u, 80u, 16u, 1u},
    {"CopySimple", "COM_original", 133u, 134u, 96u, 80u, 16u, 1u},
    {"CopyRanges", "COM_original", 136u, 137u, 264u, 80u, 16u, 1u},
    {"Reset", "COM_original", 139u, 140u, 144u, 96u, 32u, 0u},
    {"Close", "COM_original", 142u, 143u, 64u, 96u, 32u, 0u},
    {"ClearState", "COM_original", 145u, 146u, 104u, 80u, 16u, 1u},
    {"SetPipeline", "COM_original", 148u, 149u, 104u, 80u, 16u, 1u},
    {"SetRoot", "COM_original", 151u, 152u, 104u, 80u, 16u, 1u},
    {"SetHeaps", "COM_original", 154u, 155u, 192u, 80u, 16u, 1u},
    {"SetTable", "COM_original", 157u, 158u, 80u, 80u, 16u, 1u},
    {"Viewports", "COM_original", 160u, 161u, 152u, 80u, 16u, 1u},
    {"Scissors", "COM_original", 163u, 164u, 144u, 80u, 16u, 1u},
    {"RTV", "COM_original", 166u, 167u, 136u, 80u, 16u, 1u},
    {"Draw", "COM_original", 169u, 170u, 80u, 80u, 16u, 1u},
    {"BindPipeline", "Vulkan_original", 172u, 173u, 80u, 80u, 16u, 1u},
    {"EmitDraw", "Vulkan_original", 175u, 176u, 48u, 80u, 16u, 1u},
    {"Execute", "COM_original", 178u, 179u, 152u, 80u, 16u, 1u},
    {"Release", "COM_original", 181u, 182u, 64u, 96u, 32u, 0u},
    {"AllocatorReset", "COM_original", 184u, 185u, 64u, 96u, 32u, 0u},
    {"PipelineLookup", "source_original", 187u, 188u, 56u, 104u, 40u, 0u},
    {"CreateSampler", "COM_original", 190u, 191u, 168u, 80u, 16u, 1u},
    {"CreateRTV", "COM_original", 193u, 194u, 120u, 80u, 16u, 1u},
    {"SetConstants", "COM_original", 196u, 197u, 136u, 80u, 16u, 1u},
    {"Topology", "COM_original", 199u, 200u, 72u, 80u, 16u, 1u},
    {"ModuleDestroy", "Vulkan_original", 202u, 203u, 112u, 80u, 16u, 1u},
    {"PipelineDestroy", "Vulkan_original", 205u, 206u, 112u, 80u, 16u, 1u},
    {"PipelineLayoutDestroy", "Vulkan_original", 208u, 209u, 112u, 80u, 16u, 1u},
    {"ScopeGap", "source_observer", 211u, 212u, 32u, 80u, 16u, 1u},
    {"VkViewportCount", "Vulkan_original", 214u, 215u, 120u, 80u, 16u, 1u},
    {"VkScissorCount", "Vulkan_original", 217u, 218u, 112u, 80u, 16u, 1u},
};
static const char *const enum_object_kind[] = {NULL, "device", "vk_device", "resource", "heap", "allocator", "command", "queue", "root", "pso", "module", "pipeline_layout", "pipeline"};
static const char *const enum_source_site[] = {NULL, "COM_DrawInstanced", "other", "CreateCommandList", "command_constructor_reset", "default", "embedded_64_16", "embedded_32_16", "embedded_32_32", "embedded_128_32", "embedded_default", "pipeline_get", "pipeline_get_or_create", "root_graphics_layout"};
static const char *const enum_pipeline_route[] = {NULL, "legacy_monolithic", "descriptor_heap", "GPL_link", "other"};
static const char *const enum_device_origin[] = {NULL, "genuine_new", "singleton_existing", "unknown"};
static const char *const enum_scopegap_code[] = {NULL, "outside_profile", "missing_origin", "unsupported_draw_branch", "unqualified_native_route", "unsupported_root_state", "unretained_selected_original", "unsafe_projection", "quota"};
static const char *const enum_pipeline_next_kind[] = {NULL, "rendering", "flags2"};
