"""Bounded supplied SOURCE first-writer history calculation.

Normative public schema: docs/schemas/owned-source-writer-v1.json, SHA693021b2.
Pure calculation over supplied facts; no I/O, runtime instrumentation or shader-read assertion.
"""
from __future__ import annotations
import copy
import json
import math
import re
import struct

MAX_INPUT_BYTES = 65536
MAX_OUTPUT_BYTES = 262144
MAX_NODES = 4096
MAX_DEPTH = 20
MAX_STRING_BYTES = 256
_U64 = (1 << 64) - 1

_TYPES = {'U32': 'exact JSON integer0..4294967295; bool and float rejected',
 'I32': 'exact JSON integer -2147483648..2147483647; bool/float rejected',
 'U64': 'exact JSON integer0..18446744073709551615; bool/float rejected',
 'ID': 'U64 strictly>0; never0 as a known ID',
 'Bool': 'exact JSON boolean',
 'String': 'strict UTF8 string, no NUL, at most256 encoded bytes',
 'SHA256': 'String matching exactly64 lowercase hex chars',
 'Code': {'pointer': 'U64', 'bytes': 'U64', 'sha256': 'SHA256|null', 'complete': 'Bool'},
 'Ref': {'kind': 'one of '
                 'device,vk_device,resource,heap,allocator,command,queue,root,pso,module,pipeline_layout,pipeline',
         'raw': 'ID',
         'generation': 'ID'},
 'ObjectArg': {'kind': 'same enum as Ref',
               'raw': 'U64 (original0 is allowed)',
               'generation': 'ID|null (null explicitly means missing genuine birth)'},
 'Birth': {'ref': 'Ref',
           'output_field': 'one of output_raw',
           'parent': 'ObjectArg|null',
           'owner_creator_call': 'ID|null (actual retained original enclosing creator, alternative to '
                                 'already-live parent Ref)'},
 'SourceScope': {'command': 'ObjectArg|null',
                 'recording_epoch_ref': 'ID|null (actual selected Reset CallID, never APP epoch1 or a '
                                        'renumbered SOURCE reset)',
                 'resource': 'ObjectArg|null',
                 'source_site': 'null|COM_DrawInstanced|other|CreateCommandList|command_constructor_reset|default|embedded_64_16|embedded_32_16|embedded_32_32|embedded_128_32|embedded_default|pipeline_get|pipeline_get_or_create|root_graphics_layout',
                 'pipeline_owner': 'ObjectArg|null (COM pso association SOURCE bookkeeping, not helper '
                                   'state* argument)',
                 'owner_creation_call': 'ID|null (constructor owner actual original CallRef id, not a '
                                        'fabricated live Ref)',
                 'reset_ordinal': 'ID|null (genuine successful original Reset ordinal: constructor1, '
                                  'selected first-case2)',
                 'pipeline_route': 'null|legacy_monolithic|descriptor_heap|GPL_link|other (actual SOURCE '
                                   'branch bookkeeping only)'},
 'SourceAfter': {'births': 'Birth[0..1]',
                 'recording_epoch_ref': 'ID|null (selected original Reset CallID assigned only by that '
                                        'returned Reset; setup null)',
                 'command_buffer': 'U64|null (direct immutable observation only after successful original '
                                   'Reset, including constructor Reset; never fabricated CommandCreate '
                                   'output)',
                 'pso_pointer': 'U64|null (actual internal d3d12_pipeline_state* frozen only on genuine '
                                'successful PSOCreate; unrelated to COM interface numerical equality)',
                 'device_interface_pointer': 'U64|null (direct source observation '
                                             '&object->ID3D12Device_iface after genuine factory success; '
                                             'distinct from internal factory output)',
                 'device_origin': 'null|genuine_new|singleton_existing|unknown (SOURCE classification at '
                                  'actual original NEW/singleton path)',
                 'reset_ordinal': 'ID|null (new genuine successful Reset ordinal only on Reset RETURN; '
                                  'constructor1, first-case2)',
                 'root_signature_pointer': 'U64|null (only genuine successful RootCreate internal pointer '
                                           'association)',
                 'pso_root_pointer': 'U64|null (actual assigned internal root_signature* frozen only on '
                                     'successful PSOCreate)'},
 'ResourceDesc': {'dimension': 'Enum32',
                  'alignment': 'U64',
                  'width': 'U64',
                  'height': 'U32',
                  'depth_or_array_size': 'U16',
                  'mip_levels': 'U16',
                  'format': 'Enum32',
                  'sample_count': 'U32',
                  'sample_quality': 'U32',
                  'layout': 'Enum32',
                  'flags': 'U32'},
 'HeapDesc': {'heap_type': 'Enum32', 'count': 'U32', 'flags': 'U32', 'node_mask': 'U32'},
 'SRV': {'format': 'U32',
         'view_dimension': 'U32',
         'component_mapping': 'U32',
         'most_detailed_mip': 'U32',
         'mip_levels': 'U32',
         'plane_slice': 'U32',
         'min_lod_clamp_bits': 'U32'},
 'RootRange': {'range_type': 'Enum32',
               'num_descriptors': 'U32',
               'base_shader_register': 'U32',
               'register_space': 'U32',
               'offset': 'U32',
               'flags': 'U32|null'},
 'RootConstants': {'shader_register': 'U32', 'register_space': 'U32', 'num32': 'U32'},
 'RootDescriptor': {'shader_register': 'U32', 'register_space': 'U32', 'flags': 'U32|null'},
 'RootParameter': {'parameter_type': 'Enum32',
                   'visibility': 'Enum32',
                   'ranges': 'SpanRootRanges|null',
                   'constants': 'RootConstants|null',
                   'descriptor': 'RootDescriptor|null'},
 'RootDesc': {'original_version': 'Enum32',
              'flags': 'U32',
              'parameters': 'SpanRootParameters',
              'static_sampler_count': 'U32'},
 'Viewport': {'x_bits': 'U32',
              'y_bits': 'U32',
              'width_bits': 'U32',
              'height_bits': 'U32',
              'min_depth_bits': 'U32',
              'max_depth_bits': 'U32'},
 'Rect': {'left': 'I32', 'top': 'I32', 'right': 'I32', 'bottom': 'I32'},
 'RangeSide': {'count': 'U32',
               'starts_pointer': 'U64',
               'starts': 'U64[0..3]',
               'sizes_pointer': 'U64',
               'sizes': 'U32[0..3]|null'},
 'Stage': {'s_type': 'Enum32',
           'p_next': 'U64',
           'flags': 'U32',
           'stage': 'Enum32',
           'module': 'ObjectArg',
           'name_pointer': 'U64',
           'entry': 'String|null',
           'specialization_pointer': 'U64'},
 'Call': {'id': 'ID',
          'entry': 'ID',
          'return': 'ID|null',
          'parent': 'ID|null',
          'api': 'closed catalog below',
          'domain': 'catalog fixed domain',
          'source_before': 'SourceScope',
          'source_after': 'SourceAfter',
          'args': 'catalog exact object',
          'result': 'catalog exact object or null'},
 'Capture': {'context': 'ID',
             'run_id': 'String exactly32lowercasehex',
             'enabled': 'Bool',
             'faulted': 'Bool',
             'gap': 'Bool',
             'overflow': 'Bool',
             'calls_seen': 'U64',
             'original_returns': 'U64',
             'calls_dropped': 'U64',
             'pending': 'U32<=32',
             'clock': 'U64',
             'sealed_at': 'U64',
             'context_epoch': 'ID (source-context generation only; not a command Reset ordinal, APP epoch, '
                              'recording label or ticket epoch)'},
 'Ticket': {'reset_call': 'ID|null (actual retained first-case original Reset CallID)',
            'emission_call': 'ID|null',
            'execute_call': 'ID|null'},
 'Diagnostic': {'code': 'String matching[a-z0-9_]{1,64}', 'calls': 'ID[0..8]'},
 'ViewResult': {'resource': 'Ref of kind resource',
                'original_view_call': 'ID (returned original CreateSRV)',
                'history_version': 'ID (last returned typed mutation call id at Execute ENTRY)',
                'lineage_calls': 'ID[1..8] (derived copy lineage through origin CreateSRV)',
                'selected_slot': 'LogicalSlot',
                'root_layout_call': 'ID (actual original RootParse)',
                'root_parameter_index': 'U32 (actual SetTable parameter)',
                'format': 'U32',
                'most_detailed_mip': 'U32',
                'mip_count': 'U32',
                'declared_width': 'U32',
                'declared_height': 'U32',
                'binding_origin': 'literal COM_mutable_table',
                'cut': 'ID (original Execute ENTRY)'},
 'ScissorObservation': {'domain': 'literal D3D12_RECT', 'rect': 'Rect'},
 'ViewportObservation': {'domain': 'literal D3D12_VIEWPORT', 'value': 'Viewport'},
 'SpanRootParameters': {'count': 'U32',
                        'pointer': 'U64',
                        'values': 'RootParameter[0..3]',
                        'complete': 'Bool'},
 'SpanRootRanges': {'count': 'U32', 'pointer': 'U64', 'values': 'RootRange[0..1]', 'complete': 'Bool'},
 'SpanSetLayouts': {'count': 'U32', 'pointer': 'U64', 'values': 'U64[0..8]', 'complete': 'Bool'},
 'SpanStages': {'count': 'U32', 'pointer': 'U64', 'values': 'Stage[0..2]', 'complete': 'Bool'},
 'SpanHeaps': {'count': 'U32', 'pointer': 'U64', 'values': 'ObjectArg[0..2]', 'complete': 'Bool'},
 'SpanViewports': {'count': 'U32', 'pointer': 'U64', 'values': 'Viewport[0..1]', 'complete': 'Bool'},
 'SpanRects': {'count': 'U32', 'pointer': 'U64', 'values': 'Rect[0..1]', 'complete': 'Bool'},
 'SpanRTVHandles': {'count': 'U32', 'pointer': 'U64', 'values': 'U64[0..1]', 'complete': 'Bool'},
 'SpanCommands': {'count': 'U32', 'pointer': 'U64', 'values': 'ObjectArg[0..1]', 'complete': 'Bool'},
 'LogicalSlot': {'heap': 'Ref of kind heap', 'index': 'U32', 'cpu_handle': 'U64'},
 'U16': 'exact JSON integer0..65535; bool and float rejected',
 'Enum32': 'exact JSON integer -2147483648..2147483647; original 32bit enum value, no coercion to/from U32 '
           'or bool',
 'GUID': 'String exactly canonical lowercase xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx hexadecimal',
 'HeapProperties': {'type': 'Enum32',
                    'cpu_page_property': 'Enum32',
                    'memory_pool_preference': 'Enum32',
                    'creation_node_mask': 'U32',
                    'visible_node_mask': 'U32'},
 'SpanU32Words': {'count': 'U32', 'pointer': 'U64', 'values': 'U32[0..2]', 'complete': 'Bool'},
 'SpanFormats': {'count': 'U32', 'pointer': 'U64', 'values': 'Enum32[0..1]', 'complete': 'Bool'},
 'SamplerDesc': {'filter': 'Enum32',
                 'address_u': 'Enum32',
                 'address_v': 'Enum32',
                 'address_w': 'Enum32',
                 'mip_lod_bias_bits': 'U32',
                 'max_anisotropy': 'U32',
                 'comparison_func': 'Enum32',
                 'border_color_bits': 'U32[4..4]',
                 'min_lod_bits': 'U32',
                 'max_lod_bits': 'U32'},
 'PushRange': {'stage_flags': 'U32', 'offset': 'U32', 'size': 'U32'},
 'SpanPushRanges': {'count': 'U32', 'pointer': 'U64', 'values': 'PushRange[0..1]', 'complete': 'Bool'},
 'RenderingPayload': {'view_mask': 'U32',
                      'color_formats': 'SpanFormats',
                      'depth_format': 'Enum32',
                      'stencil_format': 'Enum32'},
 'Flags2Payload': {'flags': 'U64'},
 'PipelineNextNode': {'kind': 'rendering|flags2',
                      'pointer': 'U64',
                      's_type': 'Enum32',
                      'next_pointer': 'U64',
                      'payload': 'RenderingPayload when kind=rendering; Flags2Payload when kind=flags2; '
                                 'exact tagged union'},
 'PipelineNext': {'pointer': 'U64', 'nodes': 'PipelineNextNode[0..2]', 'complete': 'Bool'},
 'SpanPipelineLibraries': {'count': 'U32', 'pointer': 'U64', 'values': 'U64[0..3]', 'complete': 'Bool'},
 'LibraryProjection': {'structure_pointer': 'U64', 's_type': 'Enum32', 'libraries': 'SpanPipelineLibraries'},
 'OwnedAuxResult': {'sampler_heap': 'Ref of kind heap',
                    'sampler_table_call': 'ID',
                    'sampler_parameter': 'U32',
                    'sampler_gpu_handle': 'U64',
                    'constants_call': 'ID',
                    'constant_words': 'U32[2..2]',
                    'rtv_heap': 'Ref of kind heap',
                    'rtv_resource': 'Ref of kind resource',
                    'create_rtv_call': 'ID',
                    'rtv_bind_call': 'ID'},
 'VkRect': {'offset_x': 'I32', 'offset_y': 'I32', 'width': 'U32', 'height': 'U32'},
 'SpanVkRects': {'count': 'U32', 'pointer': 'U64', 'values': 'VkRect[0..1]', 'complete': 'Bool'},
 'VkRasterResult': {'viewport_call': 'ID', 'scissor_call': 'ID', 'viewport': 'Viewport', 'scissor': 'VkRect'},
 'PipelineStatePointers': {'vertex_input': 'U64',
                           'input_assembly': 'U64',
                           'tessellation': 'U64',
                           'viewport': 'U64',
                           'rasterization': 'U64',
                           'multisample': 'U64',
                           'depth_stencil': 'U64',
                           'color_blend': 'U64',
                           'dynamic': 'U64'}}

_CATALOG = {'DeviceCreate': {'domain': 'source_original',
                  'original_symbol': 'd3d12_device_create (genuineNEW factory path only)',
                  'args': {'output_pointer': 'U64'},
                  'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                  'return_kind': 'HRESULT_internal'},
 'VkDeviceCreate': {'domain': 'Vulkan_original',
                    'original_symbol': 'vkCreateDevice',
                    'args': {'physical_device_raw': 'U64', 'output_pointer': 'U64'},
                    'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                    'return_kind': 'VkResult'},
 'ResourceCreate': {'domain': 'COM_original',
                    'original_symbol': 'd3d12_device_CreateCommittedResource base boundary only; DESC '
                                       'original, not nested DESC1 forwarding',
                    'args': {'device': 'ObjectArg',
                             'heap_properties_pointer': 'U64',
                             'heap_properties': 'HeapProperties|null',
                             'heap_flags': 'U32',
                             'description_pointer': 'U64',
                             'description': 'ResourceDesc|null',
                             'initial_state': 'U32',
                             'clear_value_pointer': 'U64',
                             'requested_iid': 'GUID|null',
                             'output_pointer': 'U64'},
                    'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                    'return_kind': 'HRESULT'},
 'HeapCreate': {'domain': 'COM_original',
                'original_symbol': 'CreateDescriptorHeap',
                'args': {'device': 'ObjectArg',
                         'description': 'HeapDesc',
                         'output_pointer': 'U64',
                         'description_pointer': 'U64',
                         'requested_iid': 'GUID|null'},
                'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                'return_kind': 'HRESULT'},
 'AllocatorCreate': {'domain': 'COM_original',
                     'original_symbol': 'CreateCommandAllocator',
                     'args': {'device': 'ObjectArg', 'output_pointer': 'U64', 'type': 'Enum32'},
                     'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                     'return_kind': 'HRESULT'},
 'QueueCreate': {'domain': 'COM_original',
                 'original_symbol': 'CreateCommandQueue',
                 'args': {'device': 'ObjectArg', 'output_pointer': 'U64', 'type': 'Enum32'},
                 'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                 'return_kind': 'HRESULT'},
 'CommandCreate': {'domain': 'COM_original',
                   'original_symbol': 'd3d12_device_CreateCommandList base boundary only; genuine nested '
                                      'Reset is separate retained callback',
                   'args': {'device': 'ObjectArg',
                            'allocator': 'ObjectArg|null',
                            'initial_pipeline': 'ObjectArg|null',
                            'output_pointer': 'U64',
                            'node_mask': 'U32',
                            'type': 'Enum32',
                            'requested_iid': 'GUID|null'},
                   'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                   'return_kind': 'HRESULT'},
 'RootCreate': {'domain': 'COM_original',
                'original_symbol': 'CreateRootSignature',
                'args': {'device': 'ObjectArg', 'blob': 'Code', 'output_pointer': 'U64', 'node_mask': 'U32'},
                'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                'return_kind': 'HRESULT'},
 'RootParse': {'domain': 'source_original',
               'original_symbol': 'vkd3d_shader_parse_root_signature BEFORE conversion output is freed',
               'args': {'blob': 'Code', 'description_output_pointer': 'U64'},
               'result': {'status': 'I32', 'description': 'RootDesc|null'},
               'return_kind': 'vkd3d_status'},
 'PSOCreate': {'domain': 'COM_original',
               'original_symbol': 'CreateGraphicsPipelineState',
               'args': {'device': 'ObjectArg',
                        'root': 'ObjectArg',
                        'vertex_bytecode': 'Code',
                        'pixel_bytecode': 'Code',
                        'output_pointer': 'U64',
                        'rtv_formats': 'SpanFormats',
                        'sample_count': 'U32',
                        'sample_quality': 'U32'},
               'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
               'return_kind': 'HRESULT'},
 'ModuleCreate': {'domain': 'Vulkan_original',
                  'original_symbol': 'vkCreateShaderModule',
                  'args': {'device': 'ObjectArg',
                           'create_info_pointer': 'U64',
                           's_type': 'Enum32',
                           'p_next': 'U64',
                           'flags': 'U32',
                           'code': 'Code',
                           'allocator_pointer': 'U64',
                           'output_pointer': 'U64'},
                  'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                  'return_kind': 'VkResult'},
 'PipelineLayoutCreate': {'domain': 'Vulkan_original',
                          'original_symbol': 'vkCreatePipelineLayout nested only in true GRAPHICS root '
                                             'helper callsite state.c1982 (membership before tick)',
                          'args': {'device': 'ObjectArg',
                                   'create_info_pointer': 'U64',
                                   's_type': 'Enum32',
                                   'p_next': 'U64',
                                   'flags': 'U32',
                                   'set_layout_raws': 'SpanSetLayouts',
                                   'push_constant_ranges': 'SpanPushRanges',
                                   'allocator_pointer': 'U64',
                                   'output_pointer': 'U64'},
                          'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                          'return_kind': 'VkResult'},
 'PipelineCreate': {'domain': 'Vulkan_original',
                    'original_symbol': 'vkCreateGraphicsPipelines, count1 only',
                    'args': {'device': 'ObjectArg',
                             'cache_raw': 'U64',
                             'create_info_count': 'U32',
                             'create_info_pointer': 'U64',
                             'create_info_complete': 'Bool',
                             's_type': 'Enum32',
                             'flags': 'U32',
                             'p_next': 'PipelineNext',
                             'library_info': 'LibraryProjection|null',
                             'layout': 'ObjectArg|null',
                             'render_pass_raw': 'U64',
                             'subpass': 'U32',
                             'base_pipeline_raw': 'U64',
                             'base_pipeline_index': 'I32',
                             'stages': 'SpanStages',
                             'allocator_pointer': 'U64',
                             'output_pointer': 'U64',
                             'state_pointers': 'PipelineStatePointers'},
                    'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                    'return_kind': 'VkResult'},
 'Increment': {'domain': 'COM_original',
               'original_symbol': 'GetDescriptorHandleIncrementSize',
               'args': {'device': 'ObjectArg', 'heap_type': 'Enum32'},
               'result': {'value': 'U32'},
               'return_kind': 'uint32'},
 'CPUStart': {'domain': 'source_original',
              'original_symbol': 'GetCPUDescriptorHandleForHeapStart source hidden output-pointer ABI',
              'args': {'heap': 'ObjectArg', 'output_pointer': 'U64'},
              'result': {'returned_pointer': 'U64', 'value': 'U64|null'},
              'return_kind': 'original_pointer'},
 'GPUStart': {'domain': 'source_original',
              'original_symbol': 'GetGPUDescriptorHandleForHeapStart source hidden output-pointer ABI',
              'args': {'heap': 'ObjectArg', 'output_pointer': 'U64'},
              'result': {'returned_pointer': 'U64', 'value': 'U64|null'},
              'return_kind': 'original_pointer'},
 'CreateSRV': {'domain': 'COM_original',
               'original_symbol': 'CreateShaderResourceView',
               'args': {'device': 'ObjectArg',
                        'resource': 'ObjectArg|null',
                        'destination': 'U64',
                        'description': 'SRV|null',
                        'description_pointer': 'U64'},
               'result': None,
               'return_kind': 'void'},
 'CopySimple': {'domain': 'COM_original',
                'original_symbol': 'CopyDescriptorsSimple',
                'args': {'device': 'ObjectArg',
                         'count': 'U32',
                         'destination': 'U64',
                         'source': 'U64',
                         'heap_type': 'Enum32'},
                'result': None,
                'return_kind': 'void'},
 'CopyRanges': {'domain': 'COM_original',
                'original_symbol': 'CopyDescriptors',
                'args': {'device': 'ObjectArg',
                         'destinations': 'RangeSide',
                         'sources': 'RangeSide',
                         'heap_type': 'Enum32'},
                'result': None,
                'return_kind': 'void'},
 'Reset': {'domain': 'COM_original',
           'original_symbol': 'ID3D12GraphicsCommandList::Reset',
           'args': {'command': 'ObjectArg', 'allocator': 'ObjectArg', 'initial_pipeline': 'ObjectArg|null'},
           'result': {'hresult': 'I32'},
           'return_kind': 'HRESULT'},
 'Close': {'domain': 'COM_original',
           'original_symbol': 'ID3D12GraphicsCommandList::Close',
           'args': {'command': 'ObjectArg'},
           'result': {'hresult': 'I32'},
           'return_kind': 'HRESULT'},
 'ClearState': {'domain': 'COM_original',
                'original_symbol': 'ClearState',
                'args': {'command': 'ObjectArg', 'pipeline': 'ObjectArg|null'},
                'result': None,
                'return_kind': 'void'},
 'SetPipeline': {'domain': 'COM_original',
                 'original_symbol': 'SetPipelineState',
                 'args': {'command': 'ObjectArg', 'pipeline': 'ObjectArg'},
                 'result': None,
                 'return_kind': 'void'},
 'SetRoot': {'domain': 'COM_original',
             'original_symbol': 'SetGraphicsRootSignature',
             'args': {'command': 'ObjectArg', 'root': 'ObjectArg|null'},
             'result': None,
             'return_kind': 'void'},
 'SetHeaps': {'domain': 'COM_original',
              'original_symbol': 'SetDescriptorHeaps',
              'args': {'command': 'ObjectArg', 'heaps': 'SpanHeaps'},
              'result': None,
              'return_kind': 'void'},
 'SetTable': {'domain': 'COM_original',
              'original_symbol': 'SetGraphicsRootDescriptorTable original selected variant',
              'args': {'command': 'ObjectArg', 'parameter': 'U32', 'gpu_handle': 'U64'},
              'result': None,
              'return_kind': 'void'},
 'Viewports': {'domain': 'COM_original',
               'original_symbol': 'RSSetViewports',
               'args': {'command': 'ObjectArg', 'viewports': 'SpanViewports'},
               'result': None,
               'return_kind': 'void'},
 'Scissors': {'domain': 'COM_original',
              'original_symbol': 'RSSetScissorRects',
              'args': {'command': 'ObjectArg', 'rects': 'SpanRects'},
              'result': None,
              'return_kind': 'void'},
 'RTV': {'domain': 'COM_original',
         'original_symbol': 'OMSetRenderTargets',
         'args': {'command': 'ObjectArg',
                  'handles': 'SpanRTVHandles',
                  'single_range': 'I32 (original BOOL, no JSONbool coercion)',
                  'dsv_pointer': 'U64'},
         'result': None,
         'return_kind': 'void'},
 'Draw': {'domain': 'COM_original',
          'original_symbol': 'DrawInstanced',
          'args': {'command': 'ObjectArg',
                   'vertex_count': 'U32',
                   'instance_count': 'U32',
                   'first_vertex': 'U32',
                   'first_instance': 'U32'},
          'result': None,
          'return_kind': 'void'},
 'BindPipeline': {'domain': 'Vulkan_original',
                  'original_symbol': 'vkCmdBindPipeline',
                  'args': {'vk_command_buffer': 'U64', 'bind_point': 'Enum32', 'pipeline': 'ObjectArg'},
                  'result': None,
                  'return_kind': 'void'},
 'EmitDraw': {'domain': 'Vulkan_original',
              'original_symbol': 'vkCmdDraw at exact source callsite',
              'args': {'vk_command_buffer': 'U64',
                       'vertex_count': 'U32',
                       'instance_count': 'U32',
                       'first_vertex': 'U32',
                       'first_instance': 'U32'},
              'result': None,
              'return_kind': 'void'},
 'Execute': {'domain': 'COM_original',
             'original_symbol': 'ExecuteCommandLists',
             'args': {'queue': 'ObjectArg', 'commands': 'SpanCommands'},
             'result': None,
             'return_kind': 'void'},
 'Release': {'domain': 'COM_original',
             'original_symbol': 'IUnknown::Release',
             'args': {'object': 'ObjectArg'},
             'result': {'value': 'U32'},
             'return_kind': 'uint32'},
 'AllocatorReset': {'domain': 'COM_original',
                    'original_symbol': 'ID3D12CommandAllocator::Reset',
                    'args': {'allocator': 'ObjectArg'},
                    'result': {'hresult': 'I32'},
                    'return_kind': 'HRESULT'},
 'PipelineLookup': {'domain': 'source_original',
                    'original_symbol': 'd3d12_pipeline_state_get_pipeline or '
                                       'd3d12_pipeline_state_get_or_create_pipeline (source_site '
                                       'distinguishes)',
                    'args': {'state_pointer': 'U64',
                             'dynamic_state_pointer': 'U64',
                             'dsv_format_pointer': 'U64',
                             'dynamic_flags_output_pointer': 'U64'},
                    'result': {'value': 'U64', 'dynamic_flags': 'U32|null'},
                    'return_kind': 'VkPipeline'},
 'CreateSampler': {'domain': 'COM_original',
                   'original_symbol': 'CreateSampler',
                   'args': {'device': 'ObjectArg',
                            'description_pointer': 'U64',
                            'description': 'SamplerDesc|null',
                            'description_complete': 'Bool',
                            'destination': 'U64'},
                   'result': None,
                   'return_kind': 'void'},
 'CreateRTV': {'domain': 'COM_original',
               'original_symbol': 'CreateRenderTargetView',
               'args': {'device': 'ObjectArg',
                        'resource': 'ObjectArg|null',
                        'description_pointer': 'U64',
                        'destination': 'U64'},
               'result': None,
               'return_kind': 'void'},
 'SetConstants': {'domain': 'COM_original',
                  'original_symbol': 'SetGraphicsRoot32BitConstants',
                  'args': {'command': 'ObjectArg',
                           'parameter': 'U32',
                           'words': 'SpanU32Words',
                           'destination_offset': 'U32'},
                  'result': None,
                  'return_kind': 'void'},
 'Topology': {'domain': 'COM_original',
              'original_symbol': 'IASetPrimitiveTopology',
              'args': {'command': 'ObjectArg', 'topology': 'Enum32'},
              'result': None,
              'return_kind': 'void'},
 'ModuleDestroy': {'domain': 'Vulkan_original',
                   'original_symbol': 'vkDestroyShaderModule',
                   'args': {'device': 'ObjectArg',
                            'object': 'ObjectArg of kind module',
                            'allocator_pointer': 'U64'},
                   'result': None,
                   'return_kind': 'void'},
 'PipelineDestroy': {'domain': 'Vulkan_original',
                     'original_symbol': 'vkDestroyPipeline',
                     'args': {'device': 'ObjectArg',
                              'object': 'ObjectArg of kind pipeline',
                              'allocator_pointer': 'U64'},
                     'result': None,
                     'return_kind': 'void'},
 'PipelineLayoutDestroy': {'domain': 'Vulkan_original',
                           'original_symbol': 'vkDestroyPipelineLayout',
                           'args': {'device': 'ObjectArg',
                                    'object': 'ObjectArg of kind pipeline_layout',
                                    'allocator_pointer': 'U64'},
                           'result': None,
                           'return_kind': 'void'},
 'ScopeGap': {'domain': 'source_observer',
              'original_symbol': 'explicit missing/unsupported selected SOURCE observation; NOT original API '
                                 'success',
              'args': {'code': 'outside_profile|missing_origin|unsupported_draw_branch|unqualified_native_route|unsupported_root_state|unretained_selected_original|unsafe_projection|quota'},
              'result': None,
              'return_kind': 'observer'},
 'VkViewportCount': {'domain': 'Vulkan_original',
                     'original_symbol': 'vkCmdSetViewportWithCount (COM route command.c8382)',
                     'args': {'vk_command_buffer': 'U64', 'viewports': 'SpanViewports'},
                     'result': None,
                     'return_kind': 'void'},
 'VkScissorCount': {'domain': 'Vulkan_original',
                    'original_symbol': 'vkCmdSetScissorWithCount (COM route command.c8421)',
                    'args': {'vk_command_buffer': 'U64', 'rects': 'SpanVkRects'},
                    'result': None,
                    'return_kind': 'void'}}

_CLAIMS = ('input_facts_attested', 'source_runtime_coverage_proven', 'actual_sampled_input_identity_known', 'shader_uses_selected_binding_proven', 'sampled_resource_GPU_read_proven', 'current_descriptor_contents_known', 'current_input_texels_known', 'sampler_known', 'game_numeric_UV_known', 'renderer_owner_known', 'backing_known', 'loaded_binary_source_equivalence_proven', 'generic_PE_to_Unix_mapping_known', 'global_API_absence_proven', 'VR_solution_proven', 'native_descriptor_or_image_view_binding_proven', 'public_device_factory_result_known')


class _Unknown(Exception):
    def __init__(self, code, *calls):
        self.code = code
        self.calls = list(dict.fromkeys(c for c in calls if type(c) is int and c > 0))
        if len(self.calls) > 8:
            raise ValueError('Diagnostic call-ID bound')


def _require(condition, code, *calls):
    if not condition:
        raise _Unknown(code, *calls)


def _shape(value, fields, path):
    if type(value) is not dict or set(value) != set(fields):
        raise ValueError(path + ': exact object keys')


def _text(value, path):
    if type(value) is not str:
        raise ValueError(path + ': string required')
    try:
        data = value.encode('utf-8', 'strict')
    except UnicodeError as exc:
        raise ValueError(path + ': invalid Unicode') from exc
    if b'\0' in data or len(data) > MAX_STRING_BYTES:
        raise ValueError(path + ': string bound')


_NUMERIC = {'U16': (0, 65535), 'U32': (0, (1 << 32) - 1), 'I32': (-(1 << 31), (1 << 31) - 1),
            'Enum32': (-(1 << 31), (1 << 31) - 1), 'U64': (0, _U64), 'ID': (1, _U64)}
_REF_KINDS = tuple(_TYPES['Ref']['kind'].removeprefix('one of ').split(','))


def _typed(spec, value, path, enclosing=None):
    """Check this schema's closed type language, independently of causal rules."""
    if spec is None:
        if value is not None:
            raise ValueError(path + ': null required')
        return
    if type(spec) is dict:
        _shape(value, spec, path)
        for name, child in spec.items():
            _typed(child, value[name], path + '.' + name, value)
        return
    if spec.startswith('RenderingPayload when'):
        if enclosing is None or enclosing.get('kind') not in ('rendering', 'flags2'):
            raise ValueError(path + ': tagged payload')
        _typed('RenderingPayload' if enclosing['kind'] == 'rendering' else 'Flags2Payload', value, path)
        return
    short = spec.split(' (', 1)[0]
    alternatives = short.split('|')
    if value is None and 'null' in alternatives:
        return
    array = re.match(r'^([A-Za-z0-9_]+)\[([0-9]+)\.\.([0-9]+)\]', short)
    if array:
        name, low, high = array.groups()
        if type(value) is not list or not int(low) <= len(value) <= int(high):
            raise ValueError(path + ': array bound')
        for index, item in enumerate(value):
            _typed(name, item, path + '[' + str(index) + ']')
        return
    name = re.match(r'^([A-Za-z0-9_]+)', short)
    name = name.group(1) if name else ''
    if name in _NUMERIC:
        lo, hi = _NUMERIC[name]
        if type(value) is not int or not lo <= value <= hi:
            raise ValueError(path + ': ' + name)
        cap = re.search(r'<=(\d+)', short)
        if cap and value > int(cap.group(1)):
            raise ValueError(path + ': scalar cap')
    elif name == 'Bool':
        if type(value) is not bool:
            raise ValueError(path + ': boolean required')
    elif name in ('String', 'SHA256', 'GUID'):
        _text(value, path)
        if name == 'SHA256' and re.fullmatch('[0-9a-f]{64}', value) is None:
            raise ValueError(path + ': SHA256')
        if name == 'GUID' and re.fullmatch('[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}', value) is None:
            raise ValueError(path + ': GUID')
    elif name in _TYPES:
        _typed(_TYPES[name], value, path)
    elif short.startswith('one of '):
        if type(value) is not str or value not in short[7:].split(','):
            raise ValueError(path + ': enum')
    elif short == 'same enum as Ref':
        if type(value) is not str or value not in _REF_KINDS:
            raise ValueError(path + ': object kind')
    elif short.startswith('literal '):
        if value != short[8:]:
            raise ValueError(path + ': literal')
    elif type(value) is not str or value not in [s for s in alternatives if s != 'null']:
        raise ValueError(path + ': closed enum')


def _walk(value):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if count > MAX_NODES or depth > MAX_DEPTH:
            raise ValueError('JSON node/depth bound')
        if type(item) is dict:
            for key, child in item.items():
                _text(key, 'key')
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            _text(item, 'value')
        elif item is not None and type(item) not in (int, bool):
            raise ValueError('Exact JSON scalar types')
    visit(value, 0)


def _prefixes(value):
    if type(value) is dict:
        if set(value) == {'count', 'pointer', 'values', 'complete'}:
            if len(value['values']) > value['count']:
                raise ValueError('Original array prefix exceeds count')
        if set(value) == {'count', 'starts_pointer', 'starts', 'sizes_pointer', 'sizes'}:
            if len(value['starts']) > value['count'] or (value['sizes'] is not None and len(value['sizes']) > value['count']):
                raise ValueError('Original range prefix exceeds count')
        for child in value.values():
            _prefixes(child)
    elif type(value) is list:
        for child in value:
            _prefixes(child)


def _decode(raw):
    if type(raw) is str:
        try:
            data = raw.encode('utf-8', 'strict')
        except UnicodeError as exc:
            raise ValueError('Invalid UTF8') from exc
    elif type(raw) is bytes:
        data = raw
    else:
        raise ValueError('analyze accepts exact bytes or str')
    if len(data) > MAX_INPUT_BYTES:
        raise ValueError('Input byte bound before decode')
    def pairs(items):
        result = {}
        for key, item in items:
            if key in result:
                raise ValueError('Duplicate JSON key')
            result[key] = item
        return result
    def invalid_constant(_):
        raise ValueError('Nonfinite JSON constant')
    try:
        value = json.loads(data.decode('utf-8', 'strict'), object_pairs_hook=pairs, parse_constant=invalid_constant)
        _walk(value)
    except (UnicodeError, RecursionError) as exc:
        raise ValueError('JSON framing') from exc
    _shape(value, ('schema_version', 'purpose', 'capture', 'calls', 'ticket'), 'root')
    if type(value['schema_version']) is not int or value['schema_version'] != 2 or value['purpose'] != 'supplied_SOURCE_owned_first_writer_originals':
        raise ValueError('Schema/purpose')
    _typed('Capture', value['capture'], 'capture')
    _typed('Ticket', value['ticket'], 'ticket')
    if re.fullmatch('[0-9a-f]{32}', value['capture']['run_id']) is None:
        raise ValueError('Run nonce')
    rows = value['calls']
    if type(rows) is not list or len(rows) > 64:
        raise ValueError('Call record bound')
    for i, row in enumerate(rows):
        path = 'calls[' + str(i) + ']'
        _shape(row, _TYPES['Call'], path)
        for key in ('id', 'entry', 'return', 'parent'):
            _typed(_TYPES['Call'][key], row[key], path + '.' + key)
        if type(row['api']) is not str or row['api'] not in _CATALOG:
            raise ValueError(path + ': closed callback')
        cat = _CATALOG[row['api']]
        if row['domain'] != cat['domain']:
            raise ValueError(path + ': domain')
        _typed('SourceScope', row['source_before'], path + '.source_before')
        _typed('SourceAfter', row['source_after'], path + '.source_after')
        _typed(cat['args'], row['args'], path + '.args')
        _typed(None if row['return'] is None else cat['result'], row['result'], path + '.result')
        if row['api'] in ('CPUStart', 'GPUStart') and row['return'] is not None:
            a, r = row['args'], row['result']
            if r['value'] is not None and (a['output_pointer'] == 0 or r['returned_pointer'] != a['output_pointer']):
                raise ValueError('Undefined hidden getter output must be null')
        if row['api'] == 'CreateSRV' and row['args']['description_pointer'] == 0 and row['args']['description'] is not None:
            raise ValueError('NULL SRV pointer with invented description')
    _prefixes(value)
    return value


def _span(value):
    _require(value['complete'] and value['count'] == len(value['values']) and
             (value['count'] == 0 or value['pointer'] != 0), 'original_array_incomplete')
    return value['values']


def _same(a, b):
    return a is not None and b is not None and (a['kind'], a['raw'], a['generation']) == (b['kind'], b['raw'], b['generation'])


def _key(ref):
    return ref['kind'], ref['raw'], ref['generation']


def _float(bits):
    return struct.unpack('<f', struct.pack('<I', bits))[0]


_CREATES = {'DeviceCreate': 'device', 'VkDeviceCreate': 'vk_device', 'ResourceCreate': 'resource',
            'HeapCreate': 'heap', 'AllocatorCreate': 'allocator', 'QueueCreate': 'queue',
            'CommandCreate': 'command', 'RootCreate': 'root', 'PSOCreate': 'pso',
            'ModuleCreate': 'module', 'PipelineLayoutCreate': 'pipeline_layout', 'PipelineCreate': 'pipeline'}
_OWNERS = {'VkDeviceCreate': 'device', 'ResourceCreate': 'device', 'HeapCreate': 'device',
           'AllocatorCreate': 'device', 'QueueCreate': 'device', 'CommandCreate': 'device',
           'RootCreate': 'device', 'PSOCreate': 'device', 'ModuleCreate': 'pso',
           'PipelineLayoutCreate': 'root', 'PipelineCreate': 'pso'}
_METHODS = {'Reset', 'Close', 'ClearState', 'SetPipeline', 'SetRoot', 'SetHeaps', 'SetTable',
            'SetConstants', 'Topology', 'Viewports', 'Scissors', 'RTV', 'Draw'}
_NATIVE_COMMANDS = {'BindPipeline', 'EmitDraw', 'VkViewportCount', 'VkScissorCount'}
_PIPE_RELATED = {'ModuleCreate', 'PipelineCreate', 'ModuleDestroy', 'PipelineDestroy', 'PipelineLookup'} | _NATIVE_COMMANDS
_EMPTY_AFTER = {k: ([] if k == 'births' else None) for k in _TYPES['SourceAfter']}


class _Graph:
    def __init__(self, data):
        self.data = data
        self.rows = data['calls']
        self.calls = {}
        self.objects = {}
        self.kinds = {kind: [] for kind in _REF_KINDS}
        self.birth_calls = {}
        self.releases = []
        self.destroys = []
        self._structure()
        self._births()

    def call(self, ident, api=None):
        _require(ident in self.calls and (api is None or self.calls[ident]['api'] == api), 'call_reference_unavailable', ident)
        return self.calls[ident]

    def ancestors(self, row):
        result = []
        seen = set()
        while row['parent'] is not None:
            ident = row['parent']
            _require(ident not in seen, 'parent_cycle', ident)
            seen.add(ident)
            row = self.call(ident)
            result.append(row)
        return result

    def inside(self, child, parent):
        return any(row['id'] == parent['id'] for row in self.ancestors(child))

    def _structure(self):
        last_entry = 0
        edges = []
        for row in self.rows:
            _require(row['id'] not in self.calls and row['entry'] > last_entry, 'original_entry_order', row['id'])
            last_entry = row['entry']
            self.calls[row['id']] = row
            observer = row['domain'] == 'source_observer'
            if observer:
                _require(row['return'] == row['entry'] and row['source_after'] == _EMPTY_AFTER,
                         'observer_is_not_original', row['id'])
                edges.append(row['entry'])
            else:
                edges.append(row['entry'])
                if row['return'] is not None:
                    _require(row['return'] > row['entry'], 'original_return_order', row['id'])
                    edges.append(row['return'])
            self._applicable(row)
        _require(len(set(edges)) == len(edges), 'duplicate_source_transition')
        for row in self.rows:
            if row['parent'] is not None:
                p = self.call(row['parent'])
                _require(p['domain'] != 'source_observer' and p['entry'] < row['entry'] and
                         (p['return'] is None or row['entry'] < p['return']) and
                         (row['return'] is None or p['return'] is None or row['return'] < p['return']),
                         'original_parent_interval', row['id'], p['id'])
                self.ancestors(row)
        cap = self.data['capture']
        originals = [r for r in self.rows if r['domain'] != 'source_observer']
        returns = sum(r['return'] is not None for r in originals)
        pending = len(originals) - returns
        _require(cap['calls_seen'] >= len(originals) and cap['original_returns'] >= returns and
                 cap['pending'] >= pending and cap['original_returns'] <= cap['calls_seen'], 'source_counters_understate')
        _require(cap['enabled'] and not any(cap[k] for k in ('faulted', 'gap', 'overflow')) and
                 cap['calls_dropped'] == 0 and cap['pending'] == 0, 'capture_incomplete')
        _require(cap['calls_seen'] == len(originals) and cap['original_returns'] == returns and pending == 0,
                 'source_counter_equation')
        _require(cap['sealed_at'] == cap['clock'] and cap['clock'] <= 128 and
                 sorted(edges) == list(range(1, cap['clock'] + 1)), 'source_transition_coverage')
        _require(not any(r['api'] == 'ScopeGap' for r in self.rows), 'selected_scope_gap')

    def _applicable(self, row):
        api, before, after = row['api'], row['source_before'], row['source_after']
        ident = row['id']
        _require(before['resource'] is None, 'unused_resource_bookkeeping', ident)
        _require(api == 'PipelineCreate' or before['pipeline_route'] is None, 'pipeline_route_applicability', ident)
        allowed = {'births'}
        if api == 'DeviceCreate':
            allowed |= {'device_interface_pointer', 'device_origin'}
        if api == 'PSOCreate':
            allowed |= {'pso_pointer', 'pso_root_pointer'}
        if api == 'RootCreate':
            allowed.add('root_signature_pointer')
        if api == 'Reset':
            allowed |= {'reset_ordinal', 'recording_epoch_ref', 'command_buffer'}
        _require(all(after[k] is None for k in after if k not in allowed), 'source_after_applicability', ident)
        if row['return'] is None:
            _require(after == _EMPTY_AFTER, 'pending_after_facts', ident)
        elif type(row['result']) is dict and any(row['result'].get(k, 0) != 0 for k in ('hresult', 'vk_result', 'status')):
            _require(after == _EMPTY_AFTER, 'failed_after_facts', ident)
        if api not in _CREATES:
            _require(not after['births'], 'birth_on_noncreation', ident)
        if api not in _PIPE_RELATED:
            _require(before['pipeline_owner'] is None, 'pso_owner_applicability', ident)
        sites = {'CommandCreate': ('CreateCommandList',), 'Reset': (None, 'command_constructor_reset'),
                 'SetTable': ('default', 'embedded_64_16', 'embedded_32_16', 'embedded_32_32',
                              'embedded_128_32', 'embedded_default'),
                 'PipelineLookup': ('pipeline_get', 'pipeline_get_or_create'),
                 'PipelineLayoutCreate': ('root_graphics_layout',), 'Draw': (None, 'COM_DrawInstanced'),
                 'EmitDraw': ('COM_DrawInstanced', 'other')}
        _require(before['source_site'] in sites.get(api, (None,)), 'source_site_unsupported', ident)

    def _births(self):
        generations = {}
        for row in self.rows:
            api = row['api']
            result = row['result']
            born = row['source_after']['births']
            if api not in _CREATES:
                continue
            success = row['return'] is not None and result is not None and result.get('hresult', result.get('vk_result')) == 0
            if not success:
                _require(not born and (result is None or result['output_raw'] is None), 'undefined_creation_output', row['id'])
                continue
            _require(row['args']['output_pointer'] > 0 and result['output_raw'] not in (None, 0) and len(born) == 1,
                     'genuine_birth_unavailable', row['id'])
            b = born[0]
            ref = b['ref']
            _require(ref['kind'] == _CREATES[api] and ref['raw'] == result['output_raw'] and
                     b['output_field'] == 'output_raw', 'birth_output_domain', row['id'])
            _require(ref['generation'] > generations.get(ref['kind'], 0), 'generation_entry_order', row['id'])
            generations[ref['kind']] = ref['generation']
            _require(_key(ref) not in self.objects, 'duplicate_birth', row['id'])
            obj = {'ref': ref, 'row': row, 'birth': b, 'owner': None}
            self.objects[_key(ref)] = obj
            self.kinds[ref['kind']].append(obj)
            self.birth_calls[row['id']] = obj
        for obj in self.objects.values():
            row, birth = obj['row'], obj['birth']
            if row['api'] == 'DeviceCreate':
                _require(birth['parent'] is None and birth['owner_creator_call'] is None and
                         row['source_after']['device_origin'] == 'genuine_new' and
                         row['source_after']['device_interface_pointer'] not in (None, 0), 'device_origin_unavailable', row['id'])
                continue
            creator = birth['owner_creator_call']
            _require((birth['parent'] is None) != (creator is None), 'owner_reference_alternative', row['id'])
            if creator is not None:
                owner = self.birth_calls.get(creator)
                _require(owner is not None and self.inside(row, owner['row']) and
                         owner['row']['entry'] < row['entry'] < owner['row']['return'] and
                         row['source_before']['owner_creation_call'] in (None, creator), 'pending_creator_origin', row['id'], creator)
            else:
                owner = self.object(birth['parent'], row['entry'])
            _require(owner['ref']['kind'] == _OWNERS[row['api']], 'object_owner_kind', row['id'])
            obj['owner'] = owner
        self.releases = [r for r in self.rows if r['api'] == 'Release']
        self.destroys = [r for r in self.rows if r['api'] in ('ModuleDestroy', 'PipelineDestroy', 'PipelineLayoutDestroy')]
        for kind, objects in self.kinds.items():
            for i, a in enumerate(objects):
                for b in objects[i + 1:]:
                    if a['ref']['raw'] == b['ref']['raw']:
                        stop = self._final_end(a)
                        _require(stop is not None and stop < b['row']['entry'], 'raw_generation_overlap', a['row']['id'], b['row']['id'])
        caps = {'device': 1, 'vk_device': 1, 'resource': 6, 'heap': 4, 'allocator': 1, 'command': 1,
                'queue': 1, 'root': 1, 'pso': 1, 'module': 2, 'pipeline_layout': 1, 'pipeline': 1}
        _require(all(len(self.kinds[k]) <= v for k, v in caps.items()), 'object_cap_exceeded')

    def _identity(self, arg, com=False):
        if arg is None or arg['generation'] is None or arg['raw'] == 0:
            raise _Unknown('object_origin_unavailable')
        if com and arg['kind'] == 'device':
            matches = [o for o in self.kinds['device'] if o['ref']['generation'] == arg['generation'] and
                       o['row']['source_after']['device_interface_pointer'] == arg['raw']]
            _require(len(matches) == 1, 'device_com_association')
            return matches[0]
        _require(_key(arg) in self.objects, 'object_generation_unavailable')
        return self.objects[_key(arg)]

    def _for_object(self, row, obj):
        arg = row['args']['object']
        if arg['kind'] != obj['ref']['kind'] or arg['generation'] != obj['ref']['generation']:
            return False
        raw = obj['row']['source_after']['device_interface_pointer'] if arg['kind'] == 'device' and row['api'] == 'Release' else obj['ref']['raw']
        return arg['raw'] == raw

    def _final_end(self, obj):
        rows = self.releases if obj['ref']['kind'] not in ('module', 'pipeline', 'pipeline_layout') else self.destroys
        ends = [r['return'] for r in rows if self._for_object(r, obj) and r['return'] is not None and
                (r['api'] != 'Release' or r['result']['value'] == 0)]
        return min(ends) if ends else None

    def object(self, arg, at, kind=None, com=False):
        obj = self._identity(arg, com)
        _require((kind is None or obj['ref']['kind'] == kind) and obj['row']['return'] < at, 'object_not_live_at_entry', obj['row']['id'])
        rows = self.releases if obj['ref']['kind'] not in ('module', 'pipeline', 'pipeline_layout') else self.destroys
        _require(not any(self._for_object(r, obj) and r['entry'] <= at for r in rows), 'release_or_destroy_before_use', obj['row']['id'])
        return obj

    def owned(self, obj, owner):
        _require(obj['owner'] is owner, 'original_owner_mismatch', obj['row']['id'], owner['row']['id'])

    def only(self, kind):
        _require(len(self.kinds[kind]) == 1, 'required_object_count')
        return self.kinds[kind][0]

    def rows_for(self, api):
        return [r for r in self.rows if r['api'] == api]

    def scope(self, row, command, reset, ordinal=2):
        b = row['source_before']
        _require(_same(b['command'], command['ref']) and b['recording_epoch_ref'] == reset['id'] and
                 b['reset_ordinal'] == ordinal, 'source_command_scope', row['id'])

    def returned(self, row, before=None):
        _require(row['return'] is not None and (before is None or row['return'] < before), 'original_not_returned', row['id'])


def _before(first, second):
    _require(first['return'] is not None and first['return'] < second['entry'],
             'original_order', first['id'], second['id'])


def _code(value, module=False):
    _require(value['pointer'] > 0 and value['complete'] and value['sha256'] is not None and
             0 < value['bytes'] <= 1048576 and (not module or value['bytes'] % 4 == 0), 'code_projection_unavailable')


class _Engine(_Graph):
    def run(self):
        self.device, self.vk = self.only('device'), self.only('vk_device')
        self.command, self.allocator = self.only('command'), self.only('allocator')
        self.queue, self.root, self.pso = self.only('queue'), self.only('root'), self.only('pso')
        self.owned(self.vk, self.device)
        for kind in ('resource', 'heap', 'command', 'allocator', 'queue', 'root', 'pso'):
            for obj in self.kinds[kind]:
                self.owned(obj, self.device)
        self.reset = self.call(self.data['ticket']['reset_call'], 'Reset')
        self.emit = self.call(self.data['ticket']['emission_call'], 'EmitDraw')
        self.execute = self.call(self.data['ticket']['execute_call'], 'Execute')
        self.returned(self.execute)
        _require(self.execute['return'] <= self.data['capture']['sealed_at'], 'execute_after_seal')
        self.cut = self.execute['entry']
        self.receivers()
        self.profile()
        self.getters()
        self.root_layout()
        self.recording()
        self.native_pipeline()
        view = self.history()
        aux = self.auxiliary()
        return {'historical_declared_bound_view_known': True, 'origin': 'COM_mutable_table',
                'recording_join_known': True, 'execute_arguments_join_known': True,
                'auxiliary_arguments_consistent': True, 'declared_view': view,
                'auxiliary_arguments': aux,
                'recorded_viewport': {'domain': 'D3D12_VIEWPORT', 'value': self.viewport},
                'recorded_scissor': {'domain': 'D3D12_RECT', 'rect': self.scissor},
                'native_raster_arguments': {'viewport_call': self.vp_native['id'],
                                          'scissor_call': self.sc_native['id'],
                                          'viewport': self.native_viewport, 'scissor': self.native_scissor}}

    def receivers(self):
        # Validate all captured actual receivers, including an otherwise unreferenced
        # setter or release. Missing scope cannot erase a real same-raw original.
        for row in self.rows:
            api, a, t = row['api'], row['args'], row['entry']
            if 'device' in a:
                want = self.vk if row['domain'] == 'Vulkan_original' else self.device
                obj = self.object(a['device'], t, want['ref']['kind'], com=want is self.device)
                _require(obj is want, 'original_device_receiver', row['id'])
            if api in ('AllocatorCreate', 'QueueCreate'):
                _require(a['type'] == 0, 'command_type', row['id'])
            if api == 'CommandCreate':
                _require(a['node_mask'] == 0 and a['type'] == 0 and a['initial_pipeline'] is None and
                         a['requested_iid'] == '5b160d0f-ac1b-4185-8ba8-b3ae42a5a455', 'command_creation_profile', row['id'])
                _require(self.object(a['allocator'], t, 'allocator') is self.allocator, 'command_allocator', row['id'])
            if 'command' in a:
                if api == 'Reset' and row['source_before']['source_site'] == 'command_constructor_reset':
                    _require(a['command']['kind'] == 'command' and a['command']['raw'] == self.command['ref']['raw'] and
                             a['command']['generation'] is None, 'constructor_command_projection', row['id'])
                else:
                    _require(self.object(a['command'], t, 'command') is self.command, 'original_command_receiver', row['id'])
            if api in ('CPUStart', 'GPUStart'):
                self.object(a['heap'], t, 'heap')
            if api in ('CreateSRV', 'CreateRTV'):
                self.object(a['resource'], t, 'resource')
            if api in ('Reset', 'AllocatorReset'):
                _require(self.object(a['allocator'], t, 'allocator') is self.allocator, 'original_allocator_receiver', row['id'])
            if api == 'Execute':
                _require(self.object(a['queue'], t, 'queue') is self.queue, 'original_queue_receiver', row['id'])
                commands = _span(a['commands'])
                _require(len(commands) == 1 and self.object(commands[0], t, 'command') is self.command,
                         'execute_command_array', row['id'])
            if api == 'PSOCreate':
                _require(self.object(a['root'], t, 'root') is self.root, 'pso_root_receiver', row['id'])
            if api == 'SetPipeline':
                _require(self.object(a['pipeline'], t, 'pso') is self.pso, 'set_pipeline_receiver', row['id'])
            if api == 'SetRoot' and a['root'] is not None:
                _require(self.object(a['root'], t, 'root') is self.root, 'set_root_receiver', row['id'])
            if api == 'SetHeaps':
                for ref in _span(a['heaps']):
                    self.object(ref, t, 'heap')
            if api == 'BindPipeline':
                self.object(a['pipeline'], t, 'pipeline')
            if api == 'PipelineCreate' and a['layout'] is not None:
                self.object(a['layout'], t, 'pipeline_layout')
            if api in ('Release', 'ModuleDestroy', 'PipelineDestroy', 'PipelineLayoutDestroy'):
                if api == 'Release':
                    _require(a['object']['kind'] in ('device', 'resource', 'heap', 'allocator', 'command', 'queue', 'root', 'pso'),
                             'COM_release_domain', row['id'])
                obj = self._identity(a['object'], com=api == 'Release')
                _require(obj['row']['return'] < t, 'retirement_before_birth', row['id'])
                expected = {'ModuleDestroy': 'module', 'PipelineDestroy': 'pipeline',
                            'PipelineLayoutDestroy': 'pipeline_layout'}.get(api)
                _require(expected is None or obj['ref']['kind'] == expected, 'retirement_domain', row['id'])
                # A prior nonfinal Release is also ineligible, but never frees a raw.
                _require(not any(self._for_object(r, obj) and r['entry'] < t for r in self.releases + self.destroys),
                         'repeated_or_ambiguous_retirement', row['id'])
            b = row['source_before']
            if api in ('ModuleCreate', 'PipelineCreate', 'ModuleDestroy', 'PipelineDestroy'):
                pending_pso = self.inside(row, self.pso['row'])
                if pending_pso:
                    _require(b['pipeline_owner'] is None and b['command'] is None and
                             b['recording_epoch_ref'] is None and b['reset_ordinal'] is None,
                             'pending_pso_native_scope', row['id'])
                    if api in ('ModuleDestroy', 'PipelineDestroy'):
                        _require(b['owner_creation_call'] == self.pso['row']['id'],
                                 'pending_native_retirement_owner', row['id'])
                else:
                    _require(b['pipeline_owner'] is not None and
                             self.object(b['pipeline_owner'], t, 'pso') is self.pso and
                             b['owner_creation_call'] is None, 'live_pso_native_scope', row['id'])
                    enclosing_draws = [p for p in self.ancestors(row) if p['api'] == 'Draw']
                    if enclosing_draws:
                        _require(len(enclosing_draws) == 1, 'native_draw_parent_ambiguous', row['id'])
                        self.scope(row, self.command, self.reset)
                    else:
                        _require(b['command'] is None and b['recording_epoch_ref'] is None and
                                 b['reset_ordinal'] is None, 'nonrecording_native_scope', row['id'])
            if b['owner_creation_call'] is not None:
                owner = self.call(b['owner_creation_call'])
                _require(owner['api'] in _CREATES and self.inside(row, owner),
                         'owner_creator_bookkeeping', row['id'], owner['id'])
            if b['pipeline_owner'] is not None:
                owner = self._identity(b['pipeline_owner'])
                _require(owner is self.pso, 'pso_scope_owner', row['id'])
                if not self.inside(row, self.pso['row']):
                    self.object(b['pipeline_owner'], t, 'pso')
            if b['command'] is not None and api != 'Reset':
                _require(self.object(b['command'], t, 'command') is self.command,
                         'scope_command_receiver', row['id'])
            # Unknown associations on rows outside their chosen projection are not
            # ignored merely because the normal selected path uses other rows.
            if api not in _METHODS | _NATIVE_COMMANDS | {'PipelineLookup', 'ModuleCreate', 'PipelineCreate', 'ModuleDestroy', 'PipelineDestroy'}:
                _require(b['command'] is None and b['recording_epoch_ref'] is None and b['reset_ordinal'] is None,
                         'command_scope_applicability', row['id'])

    def profile(self):
        _require(len(self.kinds['resource']) == 6 and len(self.kinds['heap']) == 4, 'owned_object_counts')
        self.sampled, self.uploads, readbacks, targets = [], [], [], []
        for obj in self.kinds['resource']:
            a = obj['row']['args']
            d, hp = a['description'], a['heap_properties']
            _require(a['description_pointer'] > 0 and a['heap_properties_pointer'] > 0 and d is not None and hp is not None and
                     a['heap_flags'] == 0 and a['clear_value_pointer'] == 0 and
                     a['requested_iid'] == '696442be-a72e-4059-bc79-5b5c98040fad', 'resource_projection_profile', obj['row']['id'])
            _require(d['alignment'] == 0 and d['depth_or_array_size'] == 1 and d['sample_count'] == 1 and
                     d['sample_quality'] == 0, 'resource_shape', obj['row']['id'])
            if d['dimension'] == 3 and d['format'] == 28:
                _require(hp['type'] == 1 and a['initial_state'] == 1024 and d['layout'] == 0 and d['flags'] == 0 and
                         1 <= d['width'] <= 16384 and 1 <= d['height'] <= 16384 and
                         1 <= d['mip_levels'] <= min(15, max(d['width'], d['height']).bit_length()), 'sampled_resource_profile', obj['row']['id'])
                self.sampled.append(obj)
            elif d['dimension'] == 1:
                _require(d['format'] == 0 and d['height'] == 1 and d['mip_levels'] == 1 and d['layout'] == 1 and
                         d['flags'] == 0 and 0 < d['width'] <= 1048576, 'buffer_profile', obj['row']['id'])
                if hp['type'] == 2 and a['initial_state'] == 2755:
                    self.uploads.append(obj)
                elif hp['type'] == 3 and a['initial_state'] == 1024:
                    readbacks.append(obj)
                else:
                    raise _Unknown('buffer_heap_profile', obj['row']['id'])
            elif d['dimension'] == 3 and d['format'] == 87:
                _require(hp['type'] == 1 and a['initial_state'] == 2048 and d['width'] == 256 and d['height'] == 144 and
                         d['mip_levels'] == 1 and d['layout'] == 0 and d['flags'] == 1, 'target_profile', obj['row']['id'])
                targets.append(obj)
            else:
                raise _Unknown('resource_outside_profile', obj['row']['id'])
        _require(len(self.sampled) == 2 and len(self.uploads) == 2 and len(readbacks) == 1 and len(targets) == 1,
                 'resource_classification_ambiguous')
        self.target, self.readback = targets[0], readbacks[0]
        self.heaps = {}
        profiles = {(0, 2, 0): 'cpu', (0, 1, 1): 'visible', (1, 2, 1): 'sampler', (2, 1, 0): 'rtv'}
        for obj in self.kinds['heap']:
            a = obj['row']['args']; d = a['description']
            k = (d['heap_type'], d['count'], d['flags'])
            _require(a['description_pointer'] > 0 and a['requested_iid'] == '8efb471d-616c-4f49-90f7-127bb763fa51' and
                     d['node_mask'] == 0 and k in profiles and profiles[k] not in self.heaps, 'heap_profile', obj['row']['id'])
            self.heaps[profiles[k]] = obj
        _require(len(self.heaps) == 4, 'heap_classification')
        # Every object participating at the Execute cut must still have an eligible
        # original lifetime, including the device and auxiliary owned objects.
        for obj in self.objects.values():
            if obj['ref']['kind'] not in ('module', 'pipeline_layout', 'pipeline'):
                ref = obj['ref']
                self.object(ref, self.cut)

    def getters(self):
        rows = [r for r in self.rows if r['api'] in ('Increment', 'CPUStart', 'GPUStart')]
        _require(len(rows) == 8, 'getter_occurrence_count')
        self.inc = {}; self.cpu = {}; self.gpu = {}; self.getter_rows = {}
        for r in rows:
            self.returned(r)
            a, v = r['args'], r['result']['value']
            _require(v is not None and v > 0, 'getter_operand_unknown_or_zero', r['id'])
            if r['api'] == 'Increment':
                ht = a['heap_type']
                _require(ht in (0, 1) and ht not in self.inc, 'increment_ambiguous', r['id'])
                self.inc[ht] = v; self.getter_rows[('inc', ht)] = r
            else:
                obj = self._identity(a['heap'])
                names = [name for name, h in self.heaps.items() if h is obj]
                _require(len(names) == 1, 'getter_heap_profile', r['id'])
                name = names[0]; values = self.cpu if r['api'] == 'CPUStart' else self.gpu
                _require(name not in values and (r['api'] == 'CPUStart' or name in ('visible', 'sampler')),
                         'getter_occurrence_ambiguous', r['id'])
                values[name] = v; self.getter_rows[(r['api'], name)] = r
        _require(set(self.inc) == {0, 1} and set(self.cpu) == set(self.heaps) and set(self.gpu) == {'visible', 'sampler'},
                 'getter_profile')
        self.cpu_spans = []
        for name in ('cpu', 'visible', 'sampler'):
            ht = 1 if name == 'sampler' else 0
            count = self.heaps[name]['row']['args']['description']['count']
            end = self.cpu[name] + count * self.inc[ht]
            _require(end <= _U64, 'cpu_span_overflow')
            self.cpu_spans.append((self.cpu[name], end, name))
        for i, a in enumerate(self.cpu_spans):
            for b in self.cpu_spans[i + 1:]:
                _require(a[1] <= b[0] or b[1] <= a[0], 'cpu_span_alias')
            _require(not a[0] <= self.cpu['rtv'] < a[1], 'rtv_cpu_alias')
        spans = []
        for name in ('visible', 'sampler'):
            count = self.heaps[name]['row']['args']['description']['count']
            end = self.gpu[name] + count * self.inc[1 if name == 'sampler' else 0]
            _require(end <= (1 << 63) and self.gpu[name] < (1 << 63), 'gpu_span_overflow_or_msb')
            spans.append((self.gpu[name], end))
        _require(spans[0][1] <= spans[1][0] or spans[1][1] <= spans[0][0], 'gpu_span_alias')

    def getter_before(self, name, row, gpu=False):
        _before(self.getter_rows[('GPUStart' if gpu else 'CPUStart', name)], row)
        if name != 'rtv':
            _before(self.getter_rows[('inc', 1 if name == 'sampler' else 0)], row)

    def root_layout(self):
        rootrow = self.root['row']; a = rootrow['args']; _code(a['blob'])
        _require(a['node_mask'] == 0 and rootrow['source_after']['root_signature_pointer'] not in (None, 0), 'root_origin')
        parses = self.rows_for('RootParse')
        _require(len(parses) == 1, 'root_parse_count')
        r = parses[0]; self.rootparse = r
        _require(self.inside(r, rootrow) and r['args']['blob'] == a['blob'] and r['args']['description_output_pointer'] > 0 and
                 r['result']['status'] == 0 and r['result']['description'] is not None, 'root_parse_origin', r['id'])
        d = r['result']['description']
        _require(d['original_version'] == 1 and d['flags'] == 1 and d['static_sampler_count'] == 0, 'root_version_profile', r['id'])
        params = _span(d['parameters'])
        _require(len(params) == 3, 'root_parameter_profile', r['id'])
        for idx, p in enumerate(params):
            _require(p['visibility'] == 5 and p['descriptor'] is None, 'root_parameter_union', r['id'])
            if idx < 2:
                _require(p['parameter_type'] == 0 and p['constants'] is None and p['ranges'] is not None, 'root_table_union', r['id'])
                ranges = _span(p['ranges'])
                _require(len(ranges) == 1 and ranges[0] == {'range_type': 0 if idx == 0 else 3,
                         'num_descriptors': 1, 'base_shader_register': 0, 'register_space': 0, 'offset': 0, 'flags': None},
                         'root_table_range', r['id'])
            else:
                _require(p['parameter_type'] == 1 and p['ranges'] is None and
                         p['constants'] == {'shader_register': 0, 'register_space': 0, 'num32': 2}, 'root_constants_union', r['id'])
        p = self.pso['row']; pa = p['args']; sa = p['source_after']
        _require(sa['pso_pointer'] not in (None, 0) and sa['pso_root_pointer'] == rootrow['source_after']['root_signature_pointer'] and
                 _span(pa['rtv_formats']) == [87] and pa['sample_count'] == 1 and pa['sample_quality'] == 0, 'pso_root_association', p['id'])
        _code(pa['vertex_bytecode']); _code(pa['pixel_bytecode'])

    def recording(self):
        ctor = [r for r in self.rows_for('Reset') if r['source_before']['source_site'] == 'command_constructor_reset']
        _require(len(ctor) == 1 and len(self.rows_for('Reset')) == 2, 'first_epoch_only')
        ctor = ctor[0]; cr = self.command['row']
        _require(self.inside(ctor, cr) and ctor['source_before']['owner_creation_call'] == cr['id'] and
                 ctor['source_before']['recording_epoch_ref'] is None and ctor['source_before']['reset_ordinal'] is None and
                 ctor['result']['hresult'] == 0 and ctor['args']['initial_pipeline'] is None and
                 ctor['source_after']['reset_ordinal'] == 1 and ctor['source_after']['recording_epoch_ref'] is None and
                 ctor['source_after']['command_buffer'] not in (None, 0), 'constructor_reset_origin', ctor['id'])
        _require(_same(ctor['source_before']['command'], ctor['args']['command']), 'constructor_scope_receiver', ctor['id'])
        r = self.reset
        _require(r is not ctor and r['result']['hresult'] == 0 and r['args']['initial_pipeline'] is None and
                 r['source_before']['source_site'] is None and r['source_before']['recording_epoch_ref'] is None and
                 r['source_before']['reset_ordinal'] == 1 and _same(r['source_before']['command'], self.command['ref']) and
                 r['source_after']['reset_ordinal'] == 2 and r['source_after']['recording_epoch_ref'] == r['id'] and
                 r['source_after']['command_buffer'] not in (None, 0), 'retained_reset_origin', r['id'])
        self.cb = r['source_after']['command_buffer']
        closes = self.rows_for('Close'); exes = self.rows_for('Execute'); ars = self.rows_for('AllocatorReset')
        _require(len(closes) == 2 and len(exes) == 2 and len(ars) == 1, 'setup_and_first_execute_only')
        setup_close = next((c for c in closes if c['entry'] < r['entry']), None)
        setup_ex = next((c for c in exes if c['entry'] < r['entry']), None)
        _require(setup_close is not None and setup_ex is not None and setup_ex is not self.execute, 'setup_recording_missing')
        _before(cr, setup_close); _before(setup_close, setup_ex); _before(setup_ex, ars[0]); _before(ars[0], r)
        _require(setup_close['result']['hresult'] == 0 and ars[0]['result']['hresult'] == 0 and
                 _same(setup_close['source_before']['command'], self.command['ref']) and
                 setup_close['source_before']['recording_epoch_ref'] is None and
                 setup_close['source_before']['reset_ordinal'] == 1, 'setup_recording_state')
        draws = self.rows_for('Draw')
        _require(len(draws) == 1 and len(self.rows_for('EmitDraw')) == 1, 'single_first_writer')
        self.draw = draws[0]; d = self.draw
        _before(r, d)
        _require(self.inside(self.emit, d) and self.emit['source_before']['source_site'] == 'COM_DrawInstanced',
                 'original_emission_parent', self.emit['id'], d['id'])
        for field in ('vertex_count', 'instance_count', 'first_vertex', 'first_instance'):
            _require(d['args'][field] == self.emit['args'][field], 'draw_emission_arguments', d['id'], self.emit['id'])
        self.scope(d, self.command, r)
        selected_close = next((c for c in closes if c is not setup_close), None)
        _require(selected_close is not None and selected_close['result']['hresult'] == 0, 'selected_close')
        self.close = selected_close
        _before(d, selected_close); _before(selected_close, self.execute)
        self.scope(selected_close, self.command, r)
        setters = [c for c in self.rows if c['api'] in _METHODS - {'Reset', 'Close', 'Draw'}]
        for c in setters:
            self.scope(c, self.command, r)
            _before(r, c); _before(c, d)
        ordered = sorted(setters, key=lambda c: c['entry'])
        for x, y in zip(ordered, ordered[1:]):
            _before(x, y)
        state = {}; tables = {}; rootsame = False
        for c in ordered:
            api, a = c['api'], c['args']
            if api == 'ClearState':
                state.clear(); tables.clear(); rootsame = False
                continue
            if api == 'SetRoot':
                if a['root'] is None or not rootsame:
                    tables.clear(); state.pop('SetConstants', None)
                rootsame = a['root'] is not None and _same(a['root'], self.root['ref'])
                state[api] = c
            elif api == 'SetHeaps':
                heaps = _span(a['heaps'])
                _require(len(heaps) == 2 and _same(heaps[0], self.heaps['visible']['ref']) and
                         _same(heaps[1], self.heaps['sampler']['ref']), 'bound_heaps_profile', c['id'])
                if 'SetHeaps' not in state or state['SetHeaps']['args']['heaps']['values'] != heaps:
                    tables.clear()
                state[api] = c
            elif api == 'SetTable':
                _require(rootsame and 'SetHeaps' in state and a['parameter'] in (0, 1), 'table_without_layout_or_heaps', c['id'])
                name = 'visible' if a['parameter'] == 0 else 'sampler'
                self.getter_before(name, c, gpu=True)
                _require(a['gpu_handle'] == self.gpu[name] and a['gpu_handle'] < (1 << 63), 'table_handle_profile', c['id'])
                tables[a['parameter']] = c
            elif api == 'SetConstants':
                _require(rootsame and a['parameter'] == 2 and a['destination_offset'] == 0 and len(_span(a['words'])) == 2,
                         'constants_arguments_profile', c['id'])
                state[api] = c
            else:
                state[api] = c
        required = {'SetPipeline', 'SetRoot', 'SetHeaps', 'SetConstants', 'Topology', 'Viewports', 'Scissors', 'RTV'}
        _require(required <= set(state) and set(tables) == {0, 1} and rootsame, 'complete_recorded_state')
        self.state, self.tables = state, tables
        _require(state['Topology']['args']['topology'] == 4, 'topology_profile')
        vp = _span(state['Viewports']['args']['viewports']); sc = _span(state['Scissors']['args']['rects'])
        _require(len(vp) == len(sc) == 1, 'single_raster_profile')
        self.viewport, self.scissor = vp[0], sc[0]
        numbers = [_float(v) for v in self.viewport.values()]
        _require(all(math.isfinite(v) for v in numbers) and _float(self.viewport['width_bits']) > 0 and
                 _float(self.viewport['height_bits']) > 0, 'viewport_not_finite_or_positive')
        # Native raster is an original argument observation in a different domain,
        # not a reconstruction of COM viewport conversion or scissor clamping.
        for api in _NATIVE_COMMANDS:
            rows = self.rows_for(api)
            _require(len(rows) == 1, 'native_command_occurrence', self.emit['id'])
            c = rows[0]
            self.scope(c, self.command, r)
            _require(c['args']['vk_command_buffer'] == self.cb and self.inside(c, d) and
                     _same(c['source_before']['pipeline_owner'], self.pso['ref']), 'native_command_scope', c['id'])
            if c is not self.emit:
                _before(c, self.emit)
        self.vp_native, self.sc_native = self.rows_for('VkViewportCount')[0], self.rows_for('VkScissorCount')[0]
        nv = _span(self.vp_native['args']['viewports']); ns = _span(self.sc_native['args']['rects'])
        _require(len(nv) == len(ns) == 1, 'native_raster_count')
        _require(all(math.isfinite(_float(v)) for v in nv[0].values()), 'native_viewport_nonfinite')
        self.native_viewport, self.native_scissor = nv[0], ns[0]

    def native_pipeline(self):
        modules = self.kinds['module']; layouts = self.kinds['pipeline_layout']; pipes = self.kinds['pipeline']
        _require(len(modules) == 2 and len(layouts) == 1 and len(pipes) == 1 and
                 1 <= len(self.rows_for('PipelineCreate')) <= 2 and 1 <= len(self.rows_for('PipelineLookup')) <= 2 and
                 len(self.rows_for('ModuleDestroy')) <= 2, 'native_projection_counts')
        layout, pipeline = layouts[0], pipes[0]
        self.owned(layout, self.root); self.owned(pipeline, self.pso)
        la, lr = layout['row']['args'], layout['row']
        _require(lr['source_before']['source_site'] == 'root_graphics_layout' and la['create_info_pointer'] > 0 and
                 la['s_type'] == 30 and la['p_next'] == 0 and la['flags'] == 0 and la['allocator_pointer'] == 0,
                 'graphics_layout_original', lr['id'])
        _require(all(v > 0 for v in _span(la['set_layout_raws'])), 'layout_set_origin', lr['id'])
        for push in _span(la['push_constant_ranges']):
            _require(push['size'] > 0 and push['offset'] + push['size'] <= 0xffffffff, 'layout_push_range', lr['id'])
        for m in modules:
            self.owned(m, self.pso)
            ma = m['row']['args']
            _require(ma['create_info_pointer'] > 0 and ma['s_type'] == 16 and ma['p_next'] == 0 and
                     ma['flags'] == 0 and ma['allocator_pointer'] == 0, 'module_original_profile', m['row']['id'])
            _code(ma['code'], module=True)
        pc = pipeline['row']; a = pc['args']
        _require(pc['source_before']['pipeline_route'] == 'legacy_monolithic' and a['create_info_count'] == 1 and
                 a['create_info_pointer'] > 0 and a['create_info_complete'] and a['s_type'] == 28 and a['flags'] == 0 and
                 a['library_info'] is None and _same(a['layout'], layout['ref']) and a['render_pass_raw'] == 0 and
                 a['subpass'] == 0 and a['base_pipeline_raw'] == 0 and a['base_pipeline_index'] == -1 and
                 a['allocator_pointer'] == 0, 'monolithic_pipeline_profile', pc['id'])
        for attempt in self.rows_for('PipelineCreate'):
            _require(attempt['source_before']['pipeline_route'] == 'legacy_monolithic' and
                     (self.inside(attempt, self.pso['row']) or any(self.inside(attempt, l) for l in self.rows_for('PipelineLookup'))),
                     'pipeline_actual_creation_owner', attempt['id'])
        stages = _span(a['stages'])
        _require(len(stages) == 2 and [s['stage'] for s in stages] == [1, 16], 'pipeline_stage_profile', pc['id'])
        seen = set()
        for s in stages:
            _require(s['s_type'] == 18 and s['p_next'] == 0 and s['flags'] == 0 and s['name_pointer'] > 0 and
                     s['entry'] == 'main' and s['specialization_pointer'] == 0, 'pipeline_stage_arguments', pc['id'])
            m = self.object(s['module'], pc['entry'], 'module')
            _require(m in modules and _key(m['ref']) not in seen, 'pipeline_module_origin', pc['id'])
            seen.add(_key(m['ref']))
            _before(m['row'], pc)
            _require(not any(self._for_object(d, m) and d['entry'] < pc['return'] for d in self.destroys),
                     'module_destroy_during_pipeline_create', pc['id'])
        pn = a['p_next']; nodes = pn['nodes']
        _require(pn['complete'] and nodes and pn['pointer'] == nodes[0]['pointer'] and pn['pointer'] > 0 and
                 len(set(n['pointer'] for n in nodes)) == len(nodes) and len(set(n['kind'] for n in nodes)) == len(nodes),
                 'pipeline_next_projection', pc['id'])
        rendering = False
        for i, n in enumerate(nodes):
            _require(n['pointer'] > 0 and n['next_pointer'] == (nodes[i + 1]['pointer'] if i + 1 < len(nodes) else 0),
                     'pipeline_next_links', pc['id'])
            p = n['payload']
            if n['kind'] == 'rendering':
                _require(n['s_type'] == 1000044002 and p['view_mask'] == 0 and
                         _span(p['color_formats']) == [44] and p['depth_format'] == 0 and p['stencil_format'] == 0,
                         'pipeline_rendering_profile', pc['id'])
                rendering = True
            else:
                _require(n['s_type'] == 1000470005 and p['flags'] in (1, 0x20000000, 0x20000001),
                         'pipeline_flags2_profile', pc['id'])
        _require(rendering, 'rendering_original_missing', pc['id'])
        for obj in (layout, pipeline):
            self.object(obj['ref'], self.cut)
        lookup_rows = self.rows_for('PipelineLookup'); selected = []
        for l in lookup_rows:
            self.scope(l, self.command, self.reset)
            a = l['args']; result = l['result']
            _require(self.inside(l, self.draw) and _same(l['source_before']['pipeline_owner'], self.pso['ref']) and
                     a['state_pointer'] == self.pso['row']['source_after']['pso_pointer'] and a['state_pointer'] > 0 and
                     a['dynamic_state_pointer'] > 0 and a['dynamic_flags_output_pointer'] > 0, 'lookup_original_owner', l['id'])
            if result['value'] > 0:
                _require(result['value'] == pipeline['ref']['raw'] and result['dynamic_flags'] is not None and
                         pc['return'] < l['return'], 'lookup_defined_return', l['id'])
                selected.append(l)
            else:
                _require(result['dynamic_flags'] is None, 'failed_lookup_undefined_flags', l['id'])
        _require(len(selected) == 1, 'unique_pipeline_lookup')
        lookup = selected[0]; bind = self.rows_for('BindPipeline')[0]
        _before(lookup, bind)
        _require(bind['args']['bind_point'] == 0 and _same(bind['args']['pipeline'], pipeline['ref']), 'actual_bound_pipeline', bind['id'])
        for obj in modules + [pipeline]:
            r = obj['row']; b = r['source_before']
            if self.inside(r, self.pso['row']):
                _require(b['pipeline_owner'] is None and b['command'] is None and b['recording_epoch_ref'] is None and
                         b['reset_ordinal'] is None, 'constructor_native_scope', r['id'])
            else:
                _require(self.inside(r, lookup) and b['pipeline_owner'] is not None, 'lazy_original_birth', r['id'])
                self.scope(r, self.command, self.reset)
        for d in self.rows_for('ModuleDestroy'):
            _require(d['args']['allocator_pointer'] == 0, 'module_destroy_allocator', d['id'])
            if self.inside(d, self.pso['row']):
                _require(d['source_before']['pipeline_owner'] is None and
                         d['source_before']['owner_creation_call'] == self.pso['row']['id'], 'constructor_module_destroy_owner', d['id'])
            else:
                _require(_same(d['source_before']['pipeline_owner'], self.pso['ref']), 'module_destroy_owner', d['id'])
                if self.inside(d, self.draw):
                    self.scope(d, self.command, self.reset)

    def slot(self, handle, row, source_only=False, visible_only=False):
        matches = []
        for name in ('cpu', 'visible'):
            if source_only and name != 'cpu' or visible_only and name != 'visible':
                continue
            base, step = self.cpu[name], self.inc[0]
            count = self.heaps[name]['row']['args']['description']['count']
            delta = handle - base
            if delta >= 0 and delta % step == 0 and delta // step < count:
                matches.append((name, delta // step))
        _require(len(matches) == 1, 'descriptor_slot_resolution', row['id'])
        name, index = matches[0]
        self.getter_before(name, row)
        self.object(self.heaps[name]['ref'], row['entry'], 'heap')
        return name, index

    def ranges_unit(self, side, row):
        count = side['count']; sizes = side['sizes']
        _require(count <= 3 and count == len(side['starts']) and (count == 0 or side['starts_pointer'] > 0),
                 'copy_range_projection', row['id'])
        _require((side['sizes_pointer'] == 0 and sizes is None) or
                 (side['sizes_pointer'] > 0 and sizes is not None and len(sizes) == count), 'copy_sizes_projection', row['id'])
        expanded = []
        for base, size in zip(side['starts'], [1] * count if sizes is None else sizes):
            _require(size in (0, 1) and base + size * self.inc[0] <= _U64, 'copy_unit_range', row['id'])
            if size:
                expanded.append(base)
        _require(len(expanded) == 1, 'copy_unit_total', row['id'])
        return expanded[0]

    def history(self):
        rows = [r for r in self.rows if r['api'] in ('CreateSRV', 'CopySimple', 'CopyRanges')]
        _require(3 <= len(rows) <= 8, 'typed_mutation_count')
        writes = []
        def current(slot, cut):
            candidates = [w for w in writes if w['slot'] == slot and w['row']['entry'] < cut]
            _require(not any(w['row']['return'] >= cut for w in candidates), 'typed_write_pending_at_cut')
            _require(candidates, 'typed_source_history_missing')
            return max(candidates, key=lambda w: w['row']['return'])
        originals = []
        for row in rows:
            a = row['args']; self.returned(row)
            if row['api'] == 'CreateSRV':
                slot = self.slot(a['destination'], row)
                res = self.object(a['resource'], row['entry'], 'resource')
                d = a['description']
                _require(res in self.sampled and a['description_pointer'] > 0 and d is not None and
                         d['format'] == 28 and d['view_dimension'] == 4 and d['component_mapping'] == 5768 and
                         d['mip_levels'] == 1 and d['plane_slice'] == 0 and d['min_lod_clamp_bits'] == 0 and
                         d['most_detailed_mip'] < res['row']['args']['description']['mip_levels'], 'typed_srv_profile', row['id'])
                version = {'resource': res, 'origin': row, 'lineage': [row['id']], 'description': d}
                originals.append((row, slot, res))
            else:
                _require(a['heap_type'] == 0, 'typed_copy_heap_type', row['id'])
                if row['api'] == 'CopySimple':
                    _require(a['count'] == 1, 'typed_copy_unit_count', row['id'])
                    dst, src = a['destination'], a['source']
                else:
                    dst = self.ranges_unit(a['destinations'], row); src = self.ranges_unit(a['sources'], row)
                slot = self.slot(dst, row, visible_only=True)
                source = self.slot(src, row, source_only=True)
                frozen = current(source, row['entry'])['version']
                version = dict(frozen, lineage=frozen['lineage'] + [row['id']])
                _require(len(version['lineage']) <= 8, 'typed_lineage_bound')
            # Same-slot overlapping calls have no admitted ordering; disjoint writes
            # and a source overwrite after a copy ENTRY remain independent.
            _require(not any(w['slot'] == slot and w['row']['return'] > row['entry'] for w in writes),
                     'typed_destination_overlap', row['id'])
            writes.append({'row': row, 'slot': slot, 'version': version})
        _require(len(originals) >= 3 and [x[1] for x in originals[:3]] == [('cpu', 0), ('cpu', 1), ('visible', 0)] and
                 originals[0][2] is not originals[1][2] and originals[0][2] is originals[2][2], 'initial_owned_srv_history')
        self.initial_srvs = originals[:3]
        chosen = current(('visible', 0), self.cut); v = chosen['version']
        res, desc = v['resource'], v['description']; rd = res['row']['args']['description']
        mip = desc['most_detailed_mip']
        return {'resource': res['ref'], 'original_view_call': v['origin']['id'], 'history_version': chosen['row']['id'],
                'lineage_calls': v['lineage'], 'selected_slot': {'heap': self.heaps['visible']['ref'], 'index': 0,
                                                             'cpu_handle': self.cpu['visible']},
                'root_layout_call': self.rootparse['id'], 'root_parameter_index': self.tables[0]['args']['parameter'],
                'format': desc['format'], 'most_detailed_mip': mip, 'mip_count': 1,
                'declared_width': max(1, rd['width'] >> mip), 'declared_height': max(1, rd['height'] >> mip),
                'binding_origin': 'COM_mutable_table', 'cut': self.cut}

    def auxiliary(self):
        rows = self.rows_for('CreateSampler')
        _require(len(rows) == 2, 'sampler_argument_count')
        for idx, row in enumerate(rows):
            a = row['args']; d = a['description']; self.getter_before('sampler', row)
            _require(a['description_pointer'] > 0 and a['description_complete'] and d is not None and
                     a['destination'] == self.cpu['sampler'] + idx * self.inc[1], 'sampler_slot_arguments', row['id'])
            _require(d['filter'] == 0 and d['address_u'] == d['address_v'] == d['address_w'] == (3 if idx == 0 else 1) and
                     d['mip_lod_bias_bits'] == 0 and d['max_anisotropy'] == 1 and d['comparison_func'] == 8 and
                     d['border_color_bits'] == [0, 0, 0, 0] and d['min_lod_bits'] == d['max_lod_bits'] == 0,
                     'sampler_description_profile', row['id'])
            _before(row, self.tables[1])
        rtv = self.rows_for('CreateRTV')
        _require(len(rtv) == 1, 'rtv_creation_argument_count')
        r = rtv[0]; a = r['args']; self.getter_before('rtv', r)
        _require(self._identity(a['resource']) is self.target and a['description_pointer'] == 0 and
                 a['destination'] == self.cpu['rtv'], 'rtv_default_argument_profile', r['id'])
        bind = self.state['RTV']; _before(r, bind); self.getter_before('rtv', bind)
        _require(_span(bind['args']['handles']) == [self.cpu['rtv']] and bind['args']['single_range'] == 1 and
                 bind['args']['dsv_pointer'] == 0, 'rtv_binding_arguments', bind['id'])
        return {'sampler_heap': self.heaps['sampler']['ref'], 'sampler_table_call': self.tables[1]['id'],
                'sampler_parameter': self.tables[1]['args']['parameter'], 'sampler_gpu_handle': self.tables[1]['args']['gpu_handle'],
                'constants_call': self.state['SetConstants']['id'], 'constant_words': self.state['SetConstants']['args']['words']['values'],
                'rtv_heap': self.heaps['rtv']['ref'], 'rtv_resource': self.target['ref'], 'create_rtv_call': r['id'],
                'rtv_bind_call': bind['id']}


def _bounded_report(report):
    total = 0
    for chunk in json.JSONEncoder(ensure_ascii=False, allow_nan=False, separators=(',', ':')).iterencode(report):
        total += len(chunk.encode('utf-8', 'strict'))
        if total > MAX_OUTPUT_BYTES:
            raise ValueError('Complete output byte bound; no truncation')
    return report


def analyze(raw: bytes | str) -> dict:
    """Calculate one supplied cold first-writer history; never attest its inputs.

    Malformed framing raises ValueError. Well-typed missing, unsupported or
    contradictory SOURCE evidence returns a complete unknown diagnostic.
    """
    data = _decode(raw)
    report = {'schema_version': 2, 'purpose': 'supplied_SOURCE_owned_first_writer_historical_relation',
              'valid_source_input': True, 'historical_declared_bound_view_known': False, 'origin': None,
              'recording_join_known': False, 'execute_arguments_join_known': False,
              'auxiliary_arguments_consistent': False, 'declared_view': None, 'auxiliary_arguments': None,
              'recorded_viewport': None, 'recorded_scissor': None, 'diagnostics': [],
              'claims': {name: False for name in _CLAIMS}, 'retained_input': copy.deepcopy(data),
              'native_raster_arguments': None}
    try:
        result = _Engine(data).run()
        report.update(copy.deepcopy(result))
    except _Unknown as exc:
        report['diagnostics'].append({'code': exc.code, 'calls': exc.calls})
    return _bounded_report(report)
