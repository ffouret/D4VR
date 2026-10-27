"""Finite supplied SOURCE history calculation; no live observer or GPU claim.

Normative schema: docs/schemas/source-emitted-writer-v1.json (final3).
Only analyze(bytes|str) is public. This module performs no file/process access.
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
                 'device,vk_device,resource,vk_image,heap,allocator,command,queue,root,pso,module,pipeline_layout,pipeline,view,source_view',
         'raw': 'ID',
         'generation': 'ID'},
 'ObjectArg': {'kind': 'same enum as Ref',
               'raw': 'U64 (original0 is allowed)',
               'generation': 'ID|null (null explicitly means missing genuine birth)'},
 'Birth': {'ref': 'Ref',
           'output_field': 'one of output_raw',
           'parent': 'ObjectArg|null',
           'owner_creator_call': 'ID|null (actual retained original enclosing creator, alternative to already-live '
                                 'parent Ref)'},
 'SourceScope': {'command': 'ObjectArg|null',
                 'recording_epoch': 'ID|null',
                 'resource': 'ObjectArg|null',
                 'source_site': 'null|COM_DrawInstanced|CopyRenderPass_direct|other|CreateCommandList|CreateCommandList1|default|embedded_64_16|embedded_32_16|embedded_32_32|embedded_128_32|embedded_default|pipeline_get|pipeline_get_or_create',
                 'pipeline_owner': 'ObjectArg|null (COM pso association SOURCE bookkeeping, not helper state* '
                                   'argument)',
                 'owner_creation_call': 'ID|null (constructor owner actual original CallRef id, not a fabricated live '
                                        'Ref)'},
 'SourceAfter': {'births': 'Birth[0..1]',
                 'recording_epoch': 'ID|null',
                 'command_buffer': 'U64|null (observed source post-state only on successful Reset/CommandCreate; not '
                                   'HRESULT output)',
                 'pipeline_state_pointer': 'U64|null (direct source internal PSO object association at genuine '
                                           'successful PSOCreate; not public output)',
                 'device_interface_pointer': 'U64|null (direct source observation &object->ID3D12Device_iface after '
                                             'genuine factory success; distinct from internal factory output)',
                 'device_origin': 'null|genuine_new|singleton_existing|unknown (SOURCE classification at actual '
                                  'original NEW/singleton path)'},
 'ResourceDesc': {'dimension': 'U32',
                  'width': 'U64',
                  'height': 'U32',
                  'depth_or_array_size': 'U32',
                  'mip_levels': 'U32',
                  'format': 'U32',
                  'sample_count': 'U32',
                  'flags': 'U32'},
 'HeapDesc': {'heap_type': 'U32', 'count': 'U32', 'flags': 'U32', 'node_mask': 'U32'},
 'SRV': {'format': 'U32',
         'view_dimension': 'U32',
         'component_mapping': 'U32',
         'most_detailed_mip': 'U32',
         'mip_levels': 'U32',
         'plane_slice': 'U32',
         'min_lod_clamp_bits': 'U32'},
 'RootRange': {'range_type': 'U32',
               'num_descriptors': 'U32',
               'base_shader_register': 'U32',
               'register_space': 'U32',
               'offset': 'U32',
               'flags': 'U32|null'},
 'RootConstants': {'shader_register': 'U32', 'register_space': 'U32', 'num32': 'U32'},
 'RootDescriptor': {'shader_register': 'U32', 'register_space': 'U32', 'flags': 'U32|null'},
 'RootParameter': {'parameter_type': 'U32',
                   'visibility': 'U32',
                   'ranges': 'SpanRootRanges|null',
                   'constants': 'RootConstants|null',
                   'descriptor': 'RootDescriptor|null'},
 'RootDesc': {'original_version': 'U32',
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
 'XYZ': {'x': 'I32', 'y': 'I32', 'z': 'I32'},
 'Extent': {'width': 'U32', 'height': 'U32', 'depth': 'U32'},
 'Subresource': {'mip': 'U32', 'base_layer': 'U32', 'layer_count': 'U32', 'aspect_mask': 'U32'},
 'Box': {'left': 'U32', 'top': 'U32', 'front': 'U32', 'right': 'U32', 'bottom': 'U32', 'back': 'U32'},
 'InternalViewDesc': {'image': 'ObjectArg',
                      'view_type': 'U32',
                      'format': 'U32',
                      'base_mip': 'U32',
                      'mip_count': 'U32',
                      'base_layer': 'U32',
                      'layer_count': 'U32',
                      'aspect_mask': 'U32'},
 'CopyRegion': {'src_subresource': 'Subresource',
                'dst_subresource': 'Subresource',
                'src_offset': 'XYZ',
                'dst_offset': 'XYZ',
                'extent': 'Extent'},
 'RangeSide': {'count': 'U32',
               'starts_pointer': 'U64',
               'starts': 'U64[0..3]',
               'sizes_pointer': 'U64',
               'sizes': 'U32[0..3]|null'},
 'Stage': {'stage': 'U32', 'module': 'ObjectArg', 'entry': 'String'},
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
             'epoch': 'ID',
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
             'sealed_at': 'U64'},
 'Ticket': {'emission_call': 'ID|null', 'execute_call': 'ID|null'},
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
 'VkRect': {'offset_x': 'I32', 'offset_y': 'I32', 'width': 'U32', 'height': 'U32'},
 'U32XYZ': {'x': 'U32', 'y': 'U32', 'z': 'U32'},
 'ScissorObservation': {'domain': 'D3D12_RECT|VkRect2D', 'rect': 'Rect|null', 'vk_rect': 'VkRect|null'},
 'ViewportObservation': {'domain': 'D3D12_VIEWPORT|VkViewport', 'value': 'Viewport'},
 'SpanRootParameters': {'count': 'U32', 'pointer': 'U64', 'values': 'RootParameter[0..8]', 'complete': 'Bool'},
 'SpanRootRanges': {'count': 'U32', 'pointer': 'U64', 'values': 'RootRange[0..8]', 'complete': 'Bool'},
 'SpanSetLayouts': {'count': 'U32', 'pointer': 'U64', 'values': 'U64[0..8]', 'complete': 'Bool'},
 'SpanStages': {'count': 'U32', 'pointer': 'U64', 'values': 'Stage[0..2]', 'complete': 'Bool'},
 'SpanHeaps': {'count': 'U32', 'pointer': 'U64', 'values': 'ObjectArg[0..2]', 'complete': 'Bool'},
 'SpanViewports': {'count': 'U32', 'pointer': 'U64', 'values': 'Viewport[0..1]', 'complete': 'Bool'},
 'SpanRects': {'count': 'U32', 'pointer': 'U64', 'values': 'Rect[0..1]', 'complete': 'Bool'},
 'SpanVkRects': {'count': 'U32', 'pointer': 'U64', 'values': 'VkRect[0..1]', 'complete': 'Bool'},
 'SpanRTVHandles': {'count': 'U32', 'pointer': 'U64', 'values': 'U64[0..1]', 'complete': 'Bool'},
 'SpanCommands': {'count': 'U32', 'pointer': 'U64', 'values': 'ObjectArg[0..4]', 'complete': 'Bool'},
 'LogicalSlot': {'heap': 'Ref of kind heap', 'index': 'U32', 'cpu_handle': 'U64'}}
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
                    'original_symbol': 'CreateCommittedResource (new finite profile only)',
                    'args': {'device': 'ObjectArg', 'description': 'ResourceDesc', 'output_pointer': 'U64'},
                    'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                    'return_kind': 'HRESULT'},
 'VkImageCreate': {'domain': 'Vulkan_original',
                   'original_symbol': 'vkCreateImage',
                   'args': {'device': 'ObjectArg',
                            'image_type': 'U32',
                            'format': 'U32',
                            'extent': 'Extent',
                            'mip_levels': 'U32',
                            'array_layers': 'U32',
                            'samples': 'U32',
                            'output_pointer': 'U64'},
                   'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                   'return_kind': 'VkResult'},
 'HeapCreate': {'domain': 'COM_original',
                'original_symbol': 'CreateDescriptorHeap',
                'args': {'device': 'ObjectArg', 'description': 'HeapDesc', 'output_pointer': 'U64'},
                'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                'return_kind': 'HRESULT'},
 'AllocatorCreate': {'domain': 'COM_original',
                     'original_symbol': 'CreateCommandAllocator',
                     'args': {'device': 'ObjectArg', 'output_pointer': 'U64'},
                     'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                     'return_kind': 'HRESULT'},
 'QueueCreate': {'domain': 'COM_original',
                 'original_symbol': 'CreateCommandQueue',
                 'args': {'device': 'ObjectArg', 'output_pointer': 'U64'},
                 'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                 'return_kind': 'HRESULT'},
 'CommandCreate': {'domain': 'COM_original',
                   'original_symbol': 'CreateCommandList or CreateCommandList1, distinguished only by '
                                      'source_before.source_site',
                   'args': {'device': 'ObjectArg',
                            'allocator': 'ObjectArg|null',
                            'initial_pipeline': 'ObjectArg|null',
                            'output_pointer': 'U64'},
                   'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
                   'return_kind': 'HRESULT'},
 'RootCreate': {'domain': 'COM_original',
                'original_symbol': 'CreateRootSignature',
                'args': {'device': 'ObjectArg', 'blob': 'Code', 'output_pointer': 'U64'},
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
                        'output_pointer': 'U64'},
               'result': {'hresult': 'I32', 'output_raw': 'U64|null'},
               'return_kind': 'HRESULT'},
 'ModuleCreate': {'domain': 'Vulkan_original',
                  'original_symbol': 'vkCreateShaderModule',
                  'args': {'device': 'ObjectArg', 'code': 'Code', 'output_pointer': 'U64'},
                  'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                  'return_kind': 'VkResult'},
 'PipelineLayoutCreate': {'domain': 'Vulkan_original',
                          'original_symbol': 'vkCreatePipelineLayout',
                          'args': {'device': 'ObjectArg', 'set_layout_raws': 'SpanSetLayouts', 'output_pointer': 'U64'},
                          'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                          'return_kind': 'VkResult'},
 'PipelineCreate': {'domain': 'Vulkan_original',
                    'original_symbol': 'vkCreateGraphicsPipelines, count1 only',
                    'args': {'device': 'ObjectArg',
                             'layout': 'ObjectArg',
                             'stages': 'SpanStages',
                             'create_info_count': 'U32',
                             'create_info_pointer': 'U64',
                             'create_info_complete': 'Bool',
                             'output_pointer': 'U64'},
                    'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                    'return_kind': 'VkResult'},
 'ViewCreate': {'domain': 'Vulkan_original',
                'original_symbol': 'vkCreateImageView',
                'args': {'device': 'ObjectArg', 'description': 'InternalViewDesc', 'output_pointer': 'U64'},
                'result': {'vk_result': 'I32', 'output_raw': 'U64|null'},
                'return_kind': 'VkResult'},
 'AllocatorAddView': {'domain': 'source_original',
                      'original_symbol': 'd3d12_command_allocator_add_view',
                      'args': {'allocator': 'ObjectArg',
                               'view': 'ObjectArg of kind source_view (the original struct vkd3d_view*)'},
                      'result': {'value': 'Bool'},
                      'return_kind': 'bool'},
 'ViewLocalDecref': {'domain': 'source_original',
                     'original_symbol': 'vkd3d_view_decref (not COM lifetime end)',
                     'args': {'view': 'ObjectArg of kind source_view (the original struct vkd3d_view*)'},
                     'result': None,
                     'return_kind': 'void'},
 'Increment': {'domain': 'COM_original',
               'original_symbol': 'GetDescriptorHandleIncrementSize',
               'args': {'device': 'ObjectArg', 'heap_type': 'U32'},
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
                        'description': 'SRV|null'},
               'result': None,
               'return_kind': 'void'},
 'GetSampledImageDescriptor': {'domain': 'Vulkan_original',
                               'original_symbol': 'vkGetDescriptorEXT existing original SAMPLED_IMAGE input only',
                               'args': {'device': 'ObjectArg',
                                        'descriptor_type': 'U32',
                                        'view': 'ObjectArg',
                                        'image_layout': 'U32',
                                        'data_size': 'U64',
                                        'output_pointer': 'U64'},
                               'result': None,
                               'return_kind': 'void'},
 'CopySimple': {'domain': 'COM_original',
                'original_symbol': 'CopyDescriptorsSimple',
                'args': {'device': 'ObjectArg',
                         'count': 'U32',
                         'destination': 'U64',
                         'source': 'U64',
                         'heap_type': 'U32'},
                'result': None,
                'return_kind': 'void'},
 'CopyRanges': {'domain': 'COM_original',
                'original_symbol': 'CopyDescriptors',
                'args': {'device': 'ObjectArg',
                         'destinations': 'RangeSide',
                         'sources': 'RangeSide',
                         'heap_type': 'U32'},
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
         'args': {'command': 'ObjectArg', 'handles': 'SpanRTVHandles', 'single_range': 'Bool'},
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
 'CopyTexture': {'domain': 'COM_original',
                 'original_symbol': 'CopyTextureRegion',
                 'args': {'command': 'ObjectArg',
                          'source_resource': 'ObjectArg',
                          'destination_resource': 'ObjectArg',
                          'source_subresource': 'U32',
                          'destination_subresource': 'U32',
                          'source_box': 'Box|null',
                          'destination_offset': 'U32XYZ'},
                 'result': None,
                 'return_kind': 'void'},
 'CopyRegionObservation': {'domain': 'source_observer',
                           'original_symbol': 'explicit snapshot of original local CopyRenderPass region before view '
                                              'creation; NOT a delegate/API',
                           'args': {'parent_copy_call': 'ID',
                                    'source_resource': 'ObjectArg',
                                    'destination_resource': 'ObjectArg',
                                    'region': 'CopyRegion'},
                           'result': None,
                           'return_kind': 'observer'},
 'BindPipeline': {'domain': 'Vulkan_original',
                  'original_symbol': 'vkCmdBindPipeline',
                  'args': {'vk_command_buffer': 'U64', 'bind_point': 'U32', 'pipeline': 'ObjectArg'},
                  'result': None,
                  'return_kind': 'void'},
 'PushImage': {'domain': 'Vulkan_original',
               'original_symbol': 'vkCmdPushDescriptorSetKHR direct sampled-image route count1',
               'args': {'vk_command_buffer': 'U64',
                        'layout': 'ObjectArg',
                        'set': 'U32',
                        'binding': 'U32',
                        'array_element': 'U32',
                        'descriptor_count': 'U32',
                        'descriptor_type': 'U32',
                        'image_view': 'ObjectArg',
                        'image_layout': 'U32',
                        'sampler_raw': 'U64'},
               'result': None,
               'return_kind': 'void'},
 'VkViewport': {'domain': 'Vulkan_original',
                'original_symbol': 'vkCmdSetViewport',
                'args': {'vk_command_buffer': 'U64', 'first': 'U32', 'viewports': 'SpanViewports'},
                'result': None,
                'return_kind': 'void'},
 'VkScissor': {'domain': 'Vulkan_original',
               'original_symbol': 'vkCmdSetScissor',
               'args': {'vk_command_buffer': 'U64', 'first': 'U32', 'rects': 'SpanVkRects'},
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
 'Unsupported': {'domain': 'source_observer',
                 'original_symbol': 'explicit unsupported original callback notice; NOT a fake API success',
                 'args': {'original_name': 'String', 'affects_selected_scope': 'Bool'},
                 'result': None,
                 'return_kind': 'observer'},
 'TextureViewCreate': {'domain': 'source_original',
                       'original_symbol': 'vkd3d_create_texture_view',
                       'args': {'device': 'ObjectArg',
                                'description_projection': 'InternalViewDesc',
                                'output_pointer': 'U64'},
                       'result': {'value': 'Bool', 'output_raw': 'U64|null'},
                       'return_kind': 'bool'},
 'PipelineLookup': {'domain': 'source_original',
                    'original_symbol': 'd3d12_pipeline_state_get_pipeline or '
                                       'd3d12_pipeline_state_get_or_create_pipeline (source_site distinguishes)',
                    'args': {'state_pointer': 'U64',
                             'dynamic_state_pointer': 'U64',
                             'dsv_format_pointer': 'U64',
                             'dynamic_flags_output_pointer': 'U64'},
                    'result': {'value': 'U64', 'dynamic_flags': 'U32|null'},
                    'return_kind': 'VkPipeline'}}
_FALSE_CLAIMS = ('input_facts_attested', 'source_runtime_coverage_proven', 'actual_sampled_input_identity_known', 'shader_uses_selected_binding_proven', 'sampled_resource_GPU_read_proven', 'current_descriptor_contents_known', 'current_input_texels_known', 'sampler_known', 'game_numeric_UV_known', 'renderer_owner_known', 'backing_known', 'loaded_binary_source_equivalence_proven', 'generic_PE_to_Unix_mapping_known', 'global_API_absence_proven', 'VR_solution_proven', 'native_descriptor_or_image_view_binding_proven', 'public_device_factory_result_known')

_KINDS = tuple(x.strip() for x in _TYPES['Ref']['kind'][len('one of '):].split(','))
_KIND_CAPS = dict(device=1, vk_device=1, resource=2, vk_image=2, heap=2, allocator=1,
                  command=1, queue=1, root=1, pso=1, module=2, pipeline_layout=1,
                  pipeline=1, view=2, source_view=2)
_BIRTH_KIND = dict(DeviceCreate='device', VkDeviceCreate='vk_device', ResourceCreate='resource',
                   VkImageCreate='vk_image', HeapCreate='heap', AllocatorCreate='allocator',
                   QueueCreate='queue', CommandCreate='command', RootCreate='root', PSOCreate='pso',
                   ModuleCreate='module', PipelineLayoutCreate='pipeline_layout',
                   PipelineCreate='pipeline', ViewCreate='view', TextureViewCreate='source_view')
_COM_KINDS = {'device','resource','heap','allocator','command','queue','root','pso'}
_STATE_APIS = {'Reset','Close','ClearState','SetPipeline','SetRoot','SetHeaps','SetTable',
               'Viewports','Scissors','RTV','Draw','CopyTexture'}
_MUTATIONS = {'CreateSRV','CopySimple','CopyRanges'}


def _malformed(message):
    raise ValueError(message)


def _shape(value, fields, path):
    if type(value) is not dict or set(value) != set(fields):
        _malformed(path + ': exact object keys required')


def _enum(spec, value, path):
    # These strings are the frozen schema's finite enum descriptions, not input.
    spec = spec.split(' (', 1)[0]
    choices = spec.split('|')
    if value is None and 'null' in choices:
        return
    if type(value) is not str or value not in [x for x in choices if x != 'null']:
        _malformed(path + ': enum')


def _typed(spec, value, path):
    if value is None and ('|null' in spec or spec.startswith('null|')):
        return
    array = re.match(r'^(\w+)\[(\d+)\.\.(\d+)\]', spec)
    if array:
        item, low, high = array.groups()
        if type(value) is not list or not int(low) <= len(value) <= int(high):
            _malformed(path + ': array cap')
        for i, child in enumerate(value):
            _typed(item, child, path + '[' + str(i) + ']')
        return
    scalar = re.match(r'^(U32|I32|U64|ID|Bool|String|SHA256)\b', spec)
    if scalar:
        name = scalar.group(1)
        if name in ('U32','I32','U64','ID'):
            lo, hi = {'U32':(0,(1<<32)-1),'I32':(-(1<<31),(1<<31)-1),
                      'U64':(0,_U64),'ID':(1,_U64)}[name]
            if type(value) is not int or not lo <= value <= hi:
                _malformed(path + ': ' + name)
        elif name == 'Bool':
            if type(value) is not bool:
                _malformed(path + ': Bool')
        elif type(value) is not str:
            _malformed(path + ': String')
        elif name == 'SHA256' and re.fullmatch('[0-9a-f]{64}', value) is None:
            _malformed(path + ': SHA256')
        elif name == 'String' and 'exactly32lowercasehex' in spec and re.fullmatch('[0-9a-f]{32}', value) is None:
            _malformed(path + ': run_id')
        return
    token = spec.split(' ',1)[0].split('|',1)[0]
    if token in _TYPES and type(_TYPES[token]) is dict:
        fields = _TYPES[token]
        _shape(value, fields, path)
        for key, item_spec in fields.items():
            _typed(item_spec, value[key], path + '.' + key)
        if token.startswith('Span') and len(value['values']) > value['count']:
            _malformed(path + ': prefix exceeds actual original count')
        if token == 'RangeSide' and (len(value['starts']) > value['count'] or
                                    (value['sizes'] is not None and len(value['sizes']) > value['count'])):
            _malformed(path + ': range prefix exceeds actual original count')
        return
    if spec.startswith('one of '):
        if type(value) is not str or value not in [x.strip() for x in spec[7:].split(',')]:
            _malformed(path + ': enum')
        return
    if spec == 'same enum as Ref':
        if type(value) is not str or value not in _KINDS:
            _malformed(path + ': object kind')
        return
    if '|' in spec:
        _enum(spec, value, path)
        return
    _malformed(path + ': unsupported schema type')


def _walk(value):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if count > MAX_NODES or depth > MAX_DEPTH:
            _malformed('JSON nodes/depth cap')
        if type(item) is str:
            if '\0' in item:
                _malformed('NUL string')
            try:
                n = len(item.encode('utf-8','strict'))
            except UnicodeError as exc:
                raise ValueError('Invalid UTF8 string') from exc
            if n > MAX_STRING_BYTES:
                _malformed('String cap')
        elif type(item) is dict:
            for key, child in item.items():
                if type(key) is not str or '\0' in key or len(key.encode('utf-8')) > MAX_STRING_BYTES:
                    _malformed('Object key')
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif item is not None and type(item) not in (bool,int):
            _malformed('Only exact JSON integer scalars')
    visit(value,0)


def _decode(raw):
    if type(raw) is str:
        try:
            data = raw.encode('utf-8','strict')
        except UnicodeError as exc:
            raise ValueError('Invalid UTF8') from exc
    elif type(raw) is bytes:
        data = raw
    else:
        _malformed('analyze accepts bytes or str only')
    if len(data) > MAX_INPUT_BYTES:
        _malformed('Input byte cap before decode')
    try:
        text = data.decode('utf-8','strict')
    except UnicodeError as exc:
        raise ValueError('Invalid UTF8') from exc
    def pairs(items):
        obj = {}
        for k,v in items:
            if k in obj:
                _malformed('Duplicate JSON key')
            obj[k] = v
        return obj
    def constant(_):
        _malformed('Nonfinite JSON constant')
    try:
        d = json.loads(text, object_pairs_hook=pairs, parse_constant=constant)
        _walk(d)
    except (RecursionError,UnicodeError) as exc:
        raise ValueError('JSON framing') from exc
    _shape(d, ('schema_version','purpose','capture','calls','ticket'), 'root')
    if type(d['schema_version']) is not int or d['schema_version'] != 1 or d['purpose'] != 'supplied_SOURCE_emitted_writer_originals':
        _malformed('Schema/purpose')
    _typed('Capture',d['capture'],'capture')
    if d['capture']['pending'] > 32:
        _malformed('Pending cap')
    _typed('Ticket',d['ticket'],'ticket')
    if type(d['calls']) is not list or len(d['calls']) > 64:
        _malformed('Calls cap')
    fields = tuple(_TYPES['Call'])
    for i,c in enumerate(d['calls']):
        p = 'calls['+str(i)+']'
        _shape(c, fields, p)
        for k in ('id','entry','return','parent'):
            _typed(_TYPES['Call'][k],c[k],p+'.'+k)
        if type(c['api']) is not str or c['api'] not in _CATALOG:
            _malformed(p + ': api')
        cat = _CATALOG[c['api']]
        if c['domain'] != cat['domain']:
            _malformed(p + ': domain')
        _typed('SourceScope',c['source_before'],p+'.source_before')
        _typed('SourceAfter',c['source_after'],p+'.source_after')
        _shape(c['args'], cat['args'], p+'.args')
        for k,sp in cat['args'].items():
            _typed(sp,c['args'][k],p+'.args.'+k)
        if c['return'] is None or cat['result'] is None:
            if c['result'] is not None:
                _malformed(p + ': pending/void result must null')
        else:
            _shape(c['result'],cat['result'],p+'.result')
            for k,sp in cat['result'].items():
                _typed(sp,c['result'][k],p+'.result.'+k)
        if c['api'] in ('CPUStart','GPUStart') and c['return'] is not None:
            a,r = c['args'],c['result']
            if (a['output_pointer'] == 0 or r['returned_pointer'] != a['output_pointer']) and r['value'] is not None:
                _malformed('Undefined getter value with mismatch/null output')
    return d


class _Unknown(Exception):
    def __init__(self, code, *ids):
        self.code = code
        self.ids = [x for x in ids if type(x) is int and 0 < x <= _U64][:8]


def _need(condition, code, *ids):
    if not condition:
        raise _Unknown(code,*ids)


def _key(ref):
    return (ref['kind'],ref['raw'],ref['generation'])


def _same(a,b):
    return a is not None and b is not None and _key(a) == _key(b)


def _span(v, cap, code='original_array_incomplete'):
    _need(v['complete'] and v['count'] == len(v['values']) and v['count'] <= cap and
          (v['count'] == 0 or v['pointer'] != 0), code)
    return v['values']


def _code(v):
    _need(v['complete'] and v['pointer'] > 0 and 0 < v['bytes'] <= 1048576 and v['sha256'] is not None,
          'code_projection_unavailable')


def _success(c):
    if c['return'] is None or c['result'] is None:
        return False
    r=c['result']
    if c['api']=='TextureViewCreate':
        return r['value'] is True
    return r.get('hresult',r.get('vk_result',None)) == 0


def _ancestors(c,calls):
    while c['parent'] is not None:
        c=calls[c['parent']]
        yield c


def _report(d):
    return dict(schema_version=1,purpose='supplied_SOURCE_emitted_writer_historical_relation',
                valid_source_input=True,historical_declared_bound_view_known=False,origin=None,
                recording_join_known=False,execute_arguments_join_known=False,declared_view=None,
                original_region=None,recorded_viewport=None,recorded_scissor=None,diagnostics=[],
                claims={k:False for k in _FALSE_CLAIMS},retained_input=copy.deepcopy(d))


def _bounded(report):
    size=0
    enc=json.JSONEncoder(ensure_ascii=False,allow_nan=False,separators=(',',':'))
    for piece in enc.iterencode(report):
        size += len(piece.encode('utf-8'))
        if size > MAX_OUTPUT_BYTES:
            _malformed('Complete output byte cap; no truncation')
    return report


class _Graph:
    def __init__(self,d):
        self.d=d;self.rows=d['calls'];self.calls={};self.objects={};self.by_kind={k:[] for k in _KINDS}
        cap=d['capture']
        _need(cap['enabled'] and not (cap['faulted'] or cap['gap'] or cap['overflow'] or cap['calls_dropped']),
              'capture_unavailable')
        edges=[];previous=0;seen=returned=pending=0
        for c in self.rows:
            _need(c['id'] not in self.calls and c['entry']>previous,'call_identity_or_order',c['id'])
            self.calls[c['id']]=c;previous=c['entry'];edges.append(c['entry'])
            if c['domain']=='source_observer':
                _need(c['return']==c['entry'] and not c['source_after']['births'] and c['source_after']['recording_epoch'] is None,
                      'observer_not_original',c['id'])
            else:
                seen+=1
                if c['return'] is None:
                    pending+=1
                    _need(not c['source_after']['births'] and c['source_after']['recording_epoch'] is None,
                          'pending_publishes_birth',c['id'])
                else:
                    _need(c['return']>c['entry'],'return_order',c['id'])
                    edges.append(c['return']);returned+=1
            if c['api']=='Unsupported' and c['args']['affects_selected_scope']:
                raise _Unknown('unsupported_selected_callback',c['id'])
        _need(len(edges)<=128 and len(set(edges))==len(edges),'duplicate_or_excess_transition')
        _need(cap['clock']==cap['sealed_at'] and cap['clock']==len(edges) and
              sorted(edges)==list(range(1,len(edges)+1)),'selected_clock_gap')
        _need(cap['calls_seen']==seen and cap['original_returns']==returned and cap['pending']==pending,
              'selected_counter_mismatch')
        _need(pending==0,'pending_original')
        for c in self.rows:
            if c['parent'] is not None:
                p=self.calls.get(c['parent'])
                _need(p is not None and p['domain']!='source_observer' and p['entry']<c['entry'] and
                      p['return'] is not None and c['return'] is not None and c['return']<p['return'],
                      'parent_original_interval',c['id'])
            after=c['source_after']
            if c['api']!='DeviceCreate':
                _need(after['device_origin'] is None and after['device_interface_pointer'] is None,
                      'factory_source_field_domain',c['id'])
            if c['api']!='PSOCreate':
                _need(after['pipeline_state_pointer'] is None,'pso_source_field_domain',c['id'])
            if c['api'] not in ('Reset','CommandCreate'):
                _need(after['command_buffer'] is None and after['recording_epoch'] is None,
                      'recording_source_field_domain',c['id'])
            births=after['births']
            if births:
                _need(c['api'] in _BIRTH_KIND and _success(c) and c['args']['output_pointer']>0 and
                      c['result']['output_raw'] not in (None,0),'birth_not_defined_success',c['id'])
                b=births[0];ref=b['ref'];expected=_BIRTH_KIND[c['api']]
                raw=c['result']['output_raw']
                if c['api']=='DeviceCreate':
                    _need(after['device_origin']=='genuine_new' and after['device_interface_pointer'] not in (None,0),
                          'device_not_genuine_new',c['id'])
                    raw=after['device_interface_pointer']
                _need(ref['kind']==expected and ref['raw']==raw,'birth_output_domain',c['id'])
                k=_key(ref)
                _need(k not in self.objects and len(self.by_kind[expected])<_KIND_CAPS[expected],
                      'birth_identity_or_budget',c['id'])
                prior=self.by_kind[expected]
                _need(not prior or ref['generation']>prior[-1]['ref']['generation'],
                      'generation_not_monotonic',c['id'])
                o={'ref':ref,'call':c,'birth':b,'releases':[],'end':None,'owner':None}
                self.objects[k]=o;self.by_kind[expected].append(o)
        # Resolve actual creator references only after all eventual birth outputs are known.
        resolving=set()
        def owner(o):
            k=_key(o['ref']);b=o['birth'];c=o['call']
            _need(k not in resolving,'creator_cycle',c['id'])
            resolving.add(k)
            _need(not (b['parent'] is not None and b['owner_creator_call'] is not None),
                  'creator_owner_ambiguous',c['id'])
            parent=None
            if b['parent'] is not None:
                _need(c['source_before']['owner_creation_call'] is None,'creator_scope_disagrees',c['id'])
                parent=self.objects.get(_key(b['parent']))
                _need(parent is not None and parent['call']['return']<c['entry'],
                      'parent_not_live_before_creation',c['id'])
            elif b['owner_creator_call'] is not None:
                pc=self.calls.get(b['owner_creator_call'])
                _need(pc is not None and any(x['id']==pc['id'] for x in _ancestors(c,self.calls)),
                      'creator_not_actual_parent',c['id'])
                bs=pc['source_after']['births']
                _need(len(bs)==1,'creator_missing_successful_birth',c['id'])
                parent=self.objects.get(_key(bs[0]['ref']))
                _need(parent is not None,'creator_birth_unavailable',c['id'])
                if c['source_before']['owner_creation_call'] is not None:
                    _need(c['source_before']['owner_creation_call']==pc['id'],'creator_scope_disagrees',c['id'])
            if parent is not None:
                owner(parent)
            o['owner']=parent
            resolving.remove(k)
            return parent
        for o in self.objects.values():
            owner(o)
        for c in self.rows:
            if c['api']=='Release':
                a=c['args']['object']
                _need(a['kind'] in _COM_KINDS,'native_is_not_com_release',c['id'])
                o=self.objects.get(_key(a))
                _need(o is not None and o['call']['return']<c['entry'],'release_unknown_object',c['id'])
                if o['end'] is not None:
                    _need(c['entry']>o['end'],'release_after_retirement',c['id'])
                    raise _Unknown('duplicate_final_release',c['id'])
                o['releases'].append(c)
                if c['return'] is not None and c['result']['value']==0:
                    o['end']=c['return']
        # A final0 RETURN, not nonfinal Release, is the only reuse boundary.
        for values in self.by_kind.values():
            for i,a in enumerate(values):
                for b in values[i+1:]:
                    if a['ref']['raw']==b['ref']['raw']:
                        _need(a['end'] is not None and a['end']<b['call']['entry'],
                              'live_raw_reuse',a['call']['id'],b['call']['id'])
        for o in self.objects.values():
            c=o['call']
            if c['domain']=='COM_original' and 'device' in c['args']:
                receiver=self.obj(c['args']['device'],c['entry'],'device')
                self.owned(o,receiver)
                if c['api']=='PSOCreate':
                    original_root=self.obj(c['args']['root'],c['entry'],'root')
                    self.owned(original_root,receiver)
                if c['api']=='CommandCreate':
                    for name,kind in [('allocator','allocator'),('initial_pipeline','pso')]:
                        if c['args'][name] is not None:
                            initial=self.obj(c['args'][name],c['entry'],kind)
                            self.owned(initial,receiver)
            if o['owner'] is not None:
                p=o['owner'];c=o['call']
                if o['birth']['parent'] is not None:
                    _need(not any(x['entry']<=c['entry'] for x in p['releases']),
                          'parent_release_before_creation',c['id'])

    def call(self,ident,api=None):
        c=self.calls.get(ident)
        _need(c is not None and (api is None or c['api']==api),'selected_call_missing',ident)
        return c

    def obj(self,ref,at,kind=None):
        _need(ref is not None and ref['generation'] is not None and ref['raw']>0 and
              (kind is None or ref['kind']==kind),'object_reference_unavailable')
        o=self.objects.get(_key(ref))
        _need(o is not None and o['call']['return']<at,'object_birth_not_returned')
        _need(not any(r['entry']<=at for r in o['releases']),'release_before_selected_use',o['call']['id'])
        return o

    def owned(self,o,parent):
        _need(o['owner'] is parent,'object_owner_mismatch',o['call']['id'])

    def scope(self,c,command,epoch):
        s=c['source_before']
        _need(_same(s['command'],command['ref']) and s['recording_epoch']==epoch,
              'command_or_epoch_scope',c['id'])

    def one_getter(self,api,obj,at):
        values=[]
        for c in self.rows:
            if c['api']!=api or c['return'] is None or c['return']>=at:
                continue
            a=c['args'];r=c['result']
            if api=='Increment':
                same=a['device']['raw']==obj['ref']['raw'] and a['heap_type']==0
                if same:
                    _need(_same(a['device'],obj['ref']),'getter_object_generation',c['id'])
                known=True
            else:
                same=a['heap']['raw']==obj['ref']['raw']
                if same:
                    _need(_same(a['heap'],obj['ref']),'getter_object_generation',c['id'])
                known=a['output_pointer']>0 and r['returned_pointer']==a['output_pointer'] and r['value'] is not None
            if same and known:
                self.obj(obj['ref'],c['entry'])
                values.append(c)
        _need(len(values)==1,'getter_missing_or_ambiguous')
        c=values[0]
        _need(c['result']['value']>0,'getter_zero_not_arithmetic',c['id'])
        return c['result']['value'],c

    def desc(self,o):
        d=o['call']['args']['description']
        _need(d['dimension']==3 and 1<=d['width']<=16384 and 1<=d['height']<=16384 and
              d['depth_or_array_size']==1 and d['sample_count']==1 and d['format']==28 and
              1<=d['mip_levels']<=15 and d['mip_levels']<=max(d['width'],d['height']).bit_length(),
              'resource_profile_unsupported',o['call']['id'])
        return d

    def root_layout(self,root):
        create=root['call'];_code(create['args']['blob'])
        parses=[c for c in self.rows if c['api']=='RootParse' and
                any(p['id']==create['id'] for p in _ancestors(c,self.calls))]
        _need(len(parses)==1,'root_parse_missing_or_ambiguous',create['id'])
        c=parses[0];r=c['result']
        _need(c['return'] is not None and r['status']==0 and c['args']['description_output_pointer']>0 and
              r['description'] is not None and c['args']['blob']==create['args']['blob'],
              'root_parse_output_unavailable',c['id'])
        d=r['description']
        _need(d['original_version'] in (1,2) and d['flags'] in (0,1) and d['static_sampler_count']==0,
              'root_version_or_flags_unsupported',c['id'])
        params=_span(d['parameters'],8);tables=[]
        for i,p in enumerate(params):
            if p['parameter_type']==0:
                _need(p['ranges'] is not None and p['constants'] is None and p['descriptor'] is None and
                      p['visibility'] in (0,5),'root_table_union_or_visibility',c['id'])
                rr=_span(p['ranges'],8)
                _need(len(rr)==1,'root_extra_ranges_unsupported',c['id'])
                q=rr[0]
                _need(q['range_type']==0 and q['num_descriptors']==1 and q['offset']==0,
                      'root_range_unsupported',c['id'])
                _need(q['flags'] is None if d['original_version']==1 else q['flags']==3,
                      'root_not_explicit_mutable',c['id'])
                tables.append(i)
            elif p['parameter_type']==1:
                _need(p['ranges'] is None and p['descriptor'] is None and p['constants'] is not None and
                      p['visibility']==1 and p['constants']['num32']<=16,'root_constants_unsupported',c['id'])
            else:
                raise _Unknown('root_extra_parameter_unsupported',c['id'])
        _need(len(tables)==1,'root_table_not_unique',c['id'])
        return tables[0],c

    def recording(self,em,ex):
        draw=self.call(em['parent'],'Draw')
        cmd=self.obj(draw['args']['command'],draw['entry'],'command')
        self.obj(cmd['ref'],ex['entry'],'command')
        dev=self.obj(cmd['call']['args']['device'],cmd['call']['entry'],'device');self.owned(cmd,dev)
        self.obj(dev['ref'],ex['entry'],'device')
        _need(all(em['args'][name]==draw['args'][name] for name in ('vertex_count','instance_count','first_vertex','first_instance')),
              'emission_original_draw_args',em['id'],draw['id'])
        queue=self.obj(ex['args']['queue'],ex['entry'],'queue');self.owned(queue,dev)
        commands=_span(ex['args']['commands'],4)
        _need(len(commands)==1 and _same(commands[0],cmd['ref']),'execute_command_occurrence',ex['id'])
        create=cmd['call'];site=create['source_before']['source_site']
        _need(site in ('CreateCommandList','CreateCommandList1'),'command_creation_route',create['id'])
        open_state=site=='CreateCommandList';epoch=None;cb=None;reset=None;selected_close=None
        state={'pipeline':create['args']['initial_pipeline'],'root':None,'heaps':None,'tables':{},
               'viewport':None,'scissor':None,'rtv':None}
        relevant=[c for c in self.rows if c['api'] in _STATE_APIS and c['entry']<ex['entry'] and
                  (c['args']['command']['raw']==cmd['ref']['raw'] or
                   (c['source_before']['command'] is not None and c['source_before']['command']['raw']==cmd['ref']['raw']))]
        for i,a in enumerate(relevant):
            for b in relevant[i+1:]:
                _need(a['return']<b['entry'] or b['return']<a['entry'],
                      'overlapping_recording_state',a['id'],b['id'])
        epochs=set()
        for c in relevant:
            _need(_same(c['args']['command'],cmd['ref']),'original_command_receiver_unjoined',c['id'])
            self.obj(cmd['ref'],c['entry'],'command')
            _need(c['return'] is not None and c['return']<ex['entry'],'recording_pending_at_execute',c['id'])
            a=c['args'];api=c['api']
            if api=='Reset':
                _need(not open_state and c['result']['hresult']==0,'reset_not_closed_success',c['id'])
                al=self.obj(a['allocator'],c['entry'],'allocator');self.owned(al,dev)
                self.obj(al['ref'],ex['entry'],'allocator')
                _need(not any(r['api']=='AllocatorReset' and _same(r['args']['allocator'],al['ref']) and
                              c['return']<r['entry']<=ex['entry'] for r in self.rows),
                      'allocator_reset_before_execute',c['id'])
                if a['initial_pipeline'] is not None:
                    po=self.obj(a['initial_pipeline'],c['entry'],'pso');self.owned(po,dev)
                _need(_same(c['source_before']['command'],cmd['ref']) and
                      c['source_before']['recording_epoch']==epoch,'reset_entry_scope',c['id'])
                epoch=c['source_after']['recording_epoch'];cb=c['source_after']['command_buffer']
                _need(epoch is not None and cb not in (None,0),'reset_allocation_unavailable',c['id'])
                epochs.add(epoch);_need(len(epochs)<=1,'retained_epoch_budget',c['id'])
                open_state=True;reset=c
                state={'pipeline':a['initial_pipeline'],'root':None,'heaps':None,'tables':{},
                       'viewport':None,'scissor':None,'rtv':None}
                continue
            _need(_same(c['source_before']['command'],cmd['ref']) and
                  c['source_before']['recording_epoch']==epoch,'recording_entry_scope',c['id'])
            if api=='Close':
                _need(open_state and c['result']['hresult']==0,'close_not_open_success',c['id'])
                open_state=False
                if c['entry']>draw['return']:
                    selected_close=c
                continue
            _need(open_state,'command_operation_while_closed',c['id'])
            if c['entry']>=draw['entry']:
                # State after the selected emission cannot retag that emission.
                continue
            if api=='ClearState':
                state={'pipeline':a['pipeline'],'root':None,'heaps':None,'tables':{},
                       'viewport':None,'scissor':None,'rtv':None}
            elif api=='SetPipeline':
                state['pipeline']=a['pipeline']
            elif api=='SetRoot':
                if not _same(state['root'],a['root']):
                    state['tables']={}
                state['root']=a['root']
            elif api=='SetHeaps':
                state['heaps']=_span(a['heaps'],2)
            elif api=='SetTable':
                _need(c['source_before']['source_site'] in ('default','embedded_64_16','embedded_32_16',
                      'embedded_32_32','embedded_128_32','embedded_default'),'root_table_variant',c['id'])
                _need(state['root'] is not None,'root_table_without_root',c['id'])
                _need(a['gpu_handle']>0 and not (a['gpu_handle']>>63),'root_table_original_ignored',c['id'])
                state['tables'][a['parameter']]={'call':c,'root':state['root'],'heaps':copy.deepcopy(state['heaps'])}
            elif api=='Viewports':
                v=_span(a['viewports'],1);_need(len(v)==1,'viewport_not_single',c['id'])
                _need(all(math.isfinite(struct.unpack('<f',struct.pack('<I',x))[0]) for x in v[0].values()),
                      'viewport_nonfinite_bits',c['id'])
                state['viewport']=v[0]
            elif api=='Scissors':
                v=_span(a['rects'],1);_need(len(v)==1,'scissor_not_single',c['id']);state['scissor']=v[0]
            elif api=='RTV':
                v=_span(a['handles'],1);_need(len(v)==1 and v[0]>0,'rtv_not_single',c['id']);state['rtv']=v[0]
        _need(reset is not None and selected_close is not None and not open_state and
              reset['return']<draw['entry']<draw['return']<selected_close['entry']<selected_close['return']<ex['entry'],
              'selected_recording_not_closed',draw['id'],ex['id'])
        self.scope(draw,cmd,epoch);self.scope(em,cmd,epoch)
        _need(em['args']['vk_command_buffer']==cb,'emission_command_buffer',em['id'])
        _need(state['pipeline'] is not None and state['root'] is not None and state['heaps'] is not None and
              state['viewport'] is not None and state['scissor'] is not None and state['rtv'] is not None,
              'recorded_state_unavailable',draw['id'])
        pso=self.obj(state['pipeline'],draw['entry'],'pso');self.owned(pso,dev)
        self.obj(pso['ref'],ex['entry'],'pso')
        root=self.obj(state['root'],draw['entry'],'root');self.owned(root,dev)
        self.obj(root['ref'],ex['entry'],'root')
        _need(_same(pso['call']['args']['root'],root['ref']),'pso_root_mismatch',draw['id'])
        index,parse=self.root_layout(root)
        _need(index in state['tables'],'matching_root_parameter_missing',draw['id'])
        table=state['tables'][index]
        _need(_same(table['root'],root['ref']) and table['heaps']==state['heaps'],
              'table_heap_or_root_reassigned',table['call']['id'])
        self.pipeline(em,draw,cmd,epoch,cb,pso,root,dev,ex)
        return cmd,dev,root,index,parse,table,state

    def pipeline(self,em,draw,cmd,epoch,cb,pso,root,dev,ex):
        a=pso['call']['args'];_code(a['vertex_bytecode']);_code(a['pixel_bytecode'])
        internal=pso['call']['source_after']['pipeline_state_pointer']
        _need(internal not in (None,0),'pso_internal_association_missing',pso['call']['id'])
        binds=[c for c in self.rows if c['api']=='BindPipeline' and c['entry']<em['entry'] and
               (c['args']['vk_command_buffer']==cb or
                (c['source_before']['command'] is not None and c['source_before']['command']['raw']==cmd['ref']['raw']))]
        _need(binds,'pipeline_bind_missing',em['id'])
        b=binds[-1];self.scope(b,cmd,epoch)
        _need(b['return']<em['entry'] and b['args']['bind_point']==0 and b['args']['vk_command_buffer']==cb,
              'pipeline_bind_not_graphics_original',b['id'])
        for later in binds:
            _need(later['return']<em['entry'],'overlapping_pipeline_bind',later['id'])
        pipe=self.obj(b['args']['pipeline'],b['entry'],'pipeline');self.owned(pipe,pso)
        self.obj(pipe['ref'],ex['entry'],'pipeline')
        pc=pipe['call'];pa=pc['args']
        _need(pa['create_info_count']==1 and pa['create_info_pointer']>0 and pa['create_info_complete'],
              'pipeline_batch_projection',pc['id'])
        vkdev=self.obj(pa['device'],pc['entry'],'vk_device');self.owned(vkdev,dev)
        layout=self.obj(pa['layout'],pc['entry'],'pipeline_layout');self.owned(layout,root)
        _need(_same(layout['call']['args']['device'],vkdev['ref']),'pipeline_layout_device',pc['id'])
        _span(layout['call']['args']['set_layout_raws'],8)
        stages=_span(pa['stages'],2)
        _need(len(stages)==2 and {s['stage'] for s in stages}=={1,16},'pipeline_stages_unsupported',pc['id'])
        module_objects=[]
        for s in stages:
            _need(s['entry']=='main','pipeline_entry_unsupported',pc['id'])
            mo=self.obj(s['module'],pc['entry'],'module');self.owned(mo,pso)
            _need(_same(mo['call']['args']['device'],vkdev['ref']),'module_device_mismatch',pc['id'])
            _code(mo['call']['args']['code']);module_objects.append(mo)
        _need(len({_key(o['ref']) for o in module_objects})==2,'module_stage_occurrence_ambiguous',pc['id'])
        lookups=[]
        for c in self.rows:
            if c['api']!='PipelineLookup' or c['return'] is None or c['return']>=b['entry']:
                continue
            if c['result']['value']!=pipe['ref']['raw']:
                continue
            x=c['args']
            _need(_same(c['source_before']['pipeline_owner'],pso['ref']),'pipeline_lookup_owner_unjoined',c['id'])
            _need(c['source_before']['source_site'] in ('pipeline_get','pipeline_get_or_create') and
                  x['state_pointer']==internal and x['dynamic_state_pointer']>0 and
                  x['dynamic_flags_output_pointer']>0 and c['result']['dynamic_flags'] is not None,
                  'pipeline_lookup_outputs_or_owner',c['id'])
            self.scope(c,cmd,epoch)
            lookups.append(c)
        _need(lookups,'pipeline_lookup_original_missing',b['id'])
        lookup=max(lookups,key=lambda c:c['return'])
        ancestors=list(_ancestors(pc,self.calls))
        ahead=any(x['id']==pso['call']['id'] for x in ancestors)
        lazy=any(x['id']==lookup['id'] for x in ancestors) and any(x['id']==draw['id'] for x in _ancestors(lookup,self.calls))
        _need(ahead or lazy,'pipeline_birth_not_retained_owner_origin',pc['id'],lookup['id'])
        # A module can be born during PSO init or this genuine original lazy lookup.
        for mo in module_objects:
            path=list(_ancestors(mo['call'],self.calls))
            _need(any(x['id'] in (pso['call']['id'],lookup['id']) for x in path),
                  'module_birth_not_owner_origin',mo['call']['id'])
        _need(any(x['id']==root['call']['id'] for x in _ancestors(layout['call'],self.calls)),
              'layout_birth_not_root_origin',layout['call']['id'])

    def typed_view(self,dev,table,heaps,index,parse,cut):
        _need(len(heaps)==1,'visible_heap_bind_not_single',table['call']['id'])
        visible=self.obj(heaps[0],table['call']['entry'],'heap');self.owned(visible,dev)
        _need(len(self.by_kind['heap'])==2,'heap_profile_not_two')
        cpu=None
        for h in self.by_kind['heap']:
            self.owned(h,dev)
            hd=h['call']['args']['description']
            self.obj(h['call']['args']['device'],h['call']['entry'],'device')
            _need(hd['heap_type']==0 and hd['node_mask']==0,'heap_type_or_node_unsupported',h['call']['id'])
            if h is visible:
                _need(hd['flags']==1 and hd['count']==1,'visible_heap_profile',h['call']['id'])
            else:
                _need(hd['flags']==0 and hd['count']==2,'cpu_heap_profile',h['call']['id']);cpu=h
        _need(cpu is not None,'cpu_heap_missing')
        self.obj(visible['ref'],cut,'heap');self.obj(cpu['ref'],cut,'heap')
        getter_count=sum(c['api'] in ('CPUStart','GPUStart','Increment') for c in self.rows)
        _need(getter_count<=8,'getter_budget')
        # No CPU/GPU cross-domain numeric alias comparison is made.
        def bases(at):
            inc,_=self.one_getter('Increment',dev,at)
            spans=[]
            for h in (cpu,visible):
                base,_=self.one_getter('CPUStart',h,at)
                end=base+h['call']['args']['description']['count']*inc
                _need(end<=_U64,'cpu_span_overflow',h['call']['id'])
                spans.append((h,base,end))
            _need(spans[0][2]<=spans[1][1] or spans[1][2]<=spans[0][1],
                  'cpu_heap_spans_alias')
            return inc,spans
        def resolve(raw,at):
            inc,spans=bases(at);found=[]
            for h,base,end in spans:
                self.obj(h['ref'],at,'heap')
                if base<=raw<end and (raw-base)%inc==0:
                    found.append((h,(raw-base)//inc,raw))
            _need(len(found)==1,'slot_not_unique')
            h,i,address=found[0]
            return (_key(h['ref']),i),{'heap':h['ref'],'index':i,'cpu_handle':address},h
        t=table['call'];inc,spans=bases(t['entry'])
        gpu,_=self.one_getter('GPUStart',visible,t['entry'])
        _need(gpu<(1<<63) and gpu+inc<=_U64,'gpu_span_invalid')
        _need(t['args']['gpu_handle']==gpu,'root_table_not_visible_slot_zero',t['id'])
        vbase=next(base for h,base,end in spans if h is visible)
        selected_key,slot,_=resolve(vbase,t['entry'])
        selected={};mutations=[];slot_keys=set()
        for c in self.rows:
            if c['api'] not in _MUTATIONS or c['entry']>=cut:
                continue
            _need(_same(c['args']['device'],dev['ref']),'mutation_device_mismatch',c['id'])
            self.obj(dev['ref'],c['entry'],'device')
            a=c['args'];api=c['api'];m={'call':c,'src':None,'origin':None}
            if api=='CreateSRV':
                _need(a['resource'] is not None and a['description'] is not None,'null_typed_srv_unsupported',c['id'])
                resource=self.obj(a['resource'],c['entry'],'resource');self.owned(resource,dev)
                self.obj(resource['call']['args']['device'],resource['call']['entry'],'device')
                desc=self.desc(resource);srv=a['description']
                _need(srv['format']==28 and srv['view_dimension']==4 and srv['plane_slice']==0 and
                      srv['mip_levels']==1 and srv['most_detailed_mip']<desc['mip_levels'],
                      'srv_profile_unsupported',c['id'])
                dst,_,_=resolve(a['destination'],c['entry'])
                m['origin']={'resource':resource,'srv':srv,'view_call':c['id']}
            else:
                _need(a['heap_type']==0,'copy_heap_type_unsupported',c['id'])
                if api=='CopySimple':
                    _need(a['count']==1,'copy_count_unsupported',c['id'])
                    source,destination=a['source'],a['destination']
                else:
                    def unit(side):
                        n=side['count'];starts=side['starts'];sizes=side['sizes']
                        _need(n<=3 and len(starts)==n and (n==0 or side['starts_pointer']>0),
                              'copy_range_projection',c['id'])
                        if sizes is None:
                            _need(side['sizes_pointer']==0,'copy_null_sizes_pointer',c['id']);counts=[1]*n
                        else:
                            _need(side['sizes_pointer']>0 and len(sizes)==n,'copy_sizes_projection',c['id']);counts=sizes
                        _need(all(x in (0,1) for x in counts) and sum(counts)==1,'copy_unit_total',c['id'])
                        return next(start for start,count in zip(starts,counts) if count==1)
                    destination=unit(a['destinations']);source=unit(a['sources'])
                src,_,sh=resolve(source,c['entry']);dst,_,dh=resolve(destination,c['entry'])
                _need(sh is cpu and dh is visible,'copy_source_destination_heap_roles',c['id'])
                m['src']=src
            m['dst']=dst;m['touch']={dst}|({m['src']} if m['src'] is not None else set())
            slot_keys.update(m['touch']);mutations.append(m)
        _need(len(mutations)<=8 and len(slot_keys)<=3,'typed_mutation_or_slot_budget')
        for resource in self.by_kind['resource']:
            self.desc(resource)
        for i,a in enumerate(mutations):
            for b in mutations[i+1:]:
                if a['touch'] & b['touch']:
                    ca,cb=a['call'],b['call']
                    _need(ca['return']<cb['entry'] or cb['return']<ca['entry'],
                          'overlapping_typed_history',ca['id'],cb['id'])
        def at(which,when):
            ws=[]
            for m in mutations:
                c=m['call']
                if m['dst']!=which or c['entry']>=when:
                    continue
                _need(c['return'] is not None and c['return']<when,'typed_write_pending_at_cut',c['id'])
                ws.append(m)
            _need(ws,'slot_history_missing')
            m=max(ws,key=lambda x:x['call']['return']);c=m['call']
            if m['origin'] is not None:
                v=dict(m['origin']);v['lineage']=[c['id']]
            else:
                v=dict(at(m['src'],c['entry']));v['lineage']=[c['id']]+v['lineage']
            _need(len(v['lineage'])<=8,'copy_lineage_budget')
            v['version']=c['id'];return v
        v=at(selected_key,cut);resource=v['resource']
        self.obj(resource['ref'],cut,'resource');desc=self.desc(resource);srv=v['srv'];mip=srv['most_detailed_mip']
        selected.update(resource=copy.deepcopy(resource['ref']),original_view_call=v['view_call'],
                        history_version=v['version'],lineage_calls=v['lineage'],selected_slot=copy.deepcopy(slot),
                        root_layout_call=parse['id'],root_parameter_index=index,format=28,
                        most_detailed_mip=mip,mip_count=1,declared_width=max(1,desc['width']>>mip),
                        declared_height=max(1,desc['height']>>mip),binding_origin='COM_mutable_table',cut=cut)
        return selected


def analyze(raw: bytes | str) -> dict:
    """Calculate one bounded supplied historical declared COM binding relation.

    Framing/type/budget errors raise ValueError. Unsupported, incomplete or
    contradictory supplied histories return a full unknown report. No result
    attests the inputs, a native descriptor, runtime coverage or a GPU read.
    """
    d=_decode(raw);report=_report(d)
    selected=d['ticket']['emission_call']
    em=next((c for c in d['calls'] if c['id']==selected),None)
    if em is not None and em['api']=='EmitDraw':
        site=em['source_before']['source_site']
        report['origin']='COM_mutable_table' if site=='COM_DrawInstanced' else 'unsupported_internal'
    try:
        g=_Graph(d)
        em=g.call(selected,'EmitDraw');ex=g.call(d['ticket']['execute_call'],'Execute')
        _need(em['source_before']['source_site']=='COM_DrawInstanced','unsupported_internal_emission',em['id'])
        _need(em['return'] is not None and ex['return'] is not None,'selected_original_pending',em['id'],ex['id'])
        cmd,dev,root,index,parse,table,state=g.recording(em,ex)
        view=g.typed_view(dev,table,state['heaps'],index,parse,ex['entry'])
        report.update(historical_declared_bound_view_known=True,origin='COM_mutable_table',
                      recording_join_known=True,execute_arguments_join_known=True,declared_view=view,
                      recorded_viewport={'domain':'D3D12_VIEWPORT','value':copy.deepcopy(state['viewport'])},
                      recorded_scissor={'domain':'D3D12_RECT','rect':copy.deepcopy(state['scissor']),'vk_rect':None})
    except _Unknown as exc:
        report['diagnostics']=[{'code':exc.code,'calls':exc.ids}]
    return _bounded(report)
