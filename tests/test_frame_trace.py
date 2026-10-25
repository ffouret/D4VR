import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import frame_trace


def sample():
    view = {'image_id': 10, 'view_id': 20, 'image_known': True, 'view_known': True,
            'format': 97, 'mip_extent': [1920, 1080]}
    attachment = {'view': view, 'resolve_mode': 0, 'resolve_view': {}}
    op = {'kind': 'dynamic_rendering', 'truncated': False, 'area': [0, 0, 1920, 1080],
          'colors': [attachment], 'depth': None, 'stencil': None,
          'draws': [{'pipeline_id': 3, 'counts': [1, 4, 0, 0, 0, 0, 0, 0]}]}
    command = {'missing': False, 'truncated': False, 'operations': [op]}
    return {'schema_version': 1, 'request_sequence': 1,
            'scope': 'cpu_submission_interval_between_present_calls', 'present_before': 100,
            'present_after': 101, 'truncated': False, 'game_stereo_submitted': False,
            'pipelines': [{'id': 3, 'shaders': []}],
            'submissions': [{'complete': True, 'result': 0, 'commands': [command]}]}


class Frames(unittest.TestCase):
    def test_buffer_observations_preserve_bounds_without_admitting_aliases(self):
        data=sample();event={'kind':'descriptor_buffers','observed_event_index':0,'operation_count':0,'truncated':False,'pipeline_id':0,'draw_kind':0,
            'bind_point':0,'first_set':0,'count':1,'layout_identity_captured':False,'indices':[],'offsets':[],
            'bindings':[{'status':1,'buffer_id':8,'size':4096,'buffer_usage':0,'binding_usage':0,'address_offset':128,'remaining_bytes':3968,'creation_chain_complete':True}]}
        data['submissions'][0]['commands'][0]['resource_events']=[event]
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'frame.json';p.write_text(json.dumps(data));parsed=frame_trace.read_frame(p);summary=frame_trace.summarize(parsed)
            self.assertEqual(summary['buffer_address_status_counts'],{'unique_observed_range':1});self.assertFalse(summary['resource_aliases_excluded'])
            self.assertEqual(summary['observed_descriptor_buffer_ids'],[8])
            for key,value in [('remaining_bytes',4096),('address_offset',4096),('buffer_id',0),('status',2)]:
                old=event['bindings'][0][key];event['bindings'][0][key]=value;p.write_text(json.dumps(data))
                with self.assertRaises(ValueError):frame_trace.read_frame(p)
                event['bindings'][0][key]=old
            data['submissions'][0]['result']=-4;self.assertEqual(frame_trace.summarize(data)['observed_descriptor_buffer_ids'],[])

    def test_resource_observation_cannot_claim_contents_or_unbounded_arrays(self):
        data=sample();event={'kind':'descriptor_offsets','observed_event_index':0,'operation_count':0,'truncated':False,'pipeline_id':0,'draw_kind':0,
            'bind_point':0,'first_set':0,'count':1,'layout_identity_captured':False,'indices':[0],'offsets':[128],'bindings':[]}
        data['submissions'][0]['commands'][0]['resource_events']=[event]
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'frame.json'
            for field in ('descriptor_contents_captured','resource_aliases_excluded'):
                data[field]=True;p.write_text(json.dumps(data))
                with self.assertRaises(ValueError):frame_trace.read_frame(p)
                data[field]=False
            for key,value in [('observed_event_index',2),('layout_identity_captured',True),('offsets',[128]*33),('indices',[-1])]:
                old=event[key];event[key]=value;p.write_text(json.dumps(data))
                with self.assertRaises(ValueError):frame_trace.read_frame(p)
                event[key]=old
            data['submissions'][0]['commands'][0]['resource_events']=[event]*2049;p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):frame_trace.read_frame(p)

    def test_pipeline_declarations_are_owned_metadata_not_a_full_state_snapshot(self):
        data=sample();state={'is_library':False,'linked_libraries':False,'own_parts':15,'declarations_complete':True,
            'effective_dynamic_states_known':True,'graphics_state_snapshot_complete':False,
            'declarations':[{'source_pipeline_id':3,'parts':15,'truncated':False,'states':[0,1]}]}
        data['pipelines'][0]['pipeline_state']=state
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'frame.json';p.write_text(json.dumps(data));parsed=frame_trace.read_frame(p)
            self.assertEqual(frame_trace.summarize(parsed)['pipeline_state_declarations'][0]['declarations'][0]['states'],[0,1])
            state['linked_libraries']=True;p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):frame_trace.read_frame(p)
            state['effective_dynamic_states_known']=False;p.write_text(json.dumps(data));frame_trace.read_frame(p)
            state['declarations'][0]['states']=list(range(65));p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):frame_trace.read_frame(p)

    def test_clear_bits_require_clear_load_op_and_four_unsigned_words(self):
        data=sample();a=data['submissions'][0]['commands'][0]['operations'][0]['colors'][0]
        a.update(load=1,store=0,clear_value_words=[0,0,0,0x3f800000])
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'frame.json';p.write_text(json.dumps(data));frame_trace.read_frame(p)
            r=frame_trace.summarize(data);self.assertEqual(r['passes'][0]['attachments'][0]['clear_value_words'][3],0x3f800000)
            for words in ([0,0,0],[0,0,0,-1],[0,0,0,True]):
                a['clear_value_words']=words;p.write_text(json.dumps(data))
                with self.assertRaises(ValueError):frame_trace.read_frame(p)
            a.update(load=0,clear_value_words=[0,0,0,0]);p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):frame_trace.read_frame(p)

    def test_scopes_are_not_claimed_as_scene_identification(self):
        result = frame_trace.summarize(sample())
        self.assertEqual(result['draw_command_counts']['indexed'], 4)
        self.assertEqual(result['image_attachment_scopes'], {'10': [0]})
        self.assertFalse(result['scene_pass_identified'])
        self.assertFalse(result['gpu_execution_order_known'])
        self.assertFalse(result['game_stereo_submitted'])

    def test_failed_submissions_excluded_and_partial_coverage_retained(self):
        data = sample()
        failed = copy.deepcopy(data['submissions'][0]); failed['result'] = -4
        data['submissions'].append(failed)
        command = data['submissions'][0]['commands'][0]
        command['truncated'] = True; command['operations'][0]['draws'][0]['pipeline_id'] = 0
        result = frame_trace.summarize(data)
        self.assertTrue(result['truncated'])
        self.assertEqual(result['failed_or_incomplete_submissions'], 1)
        self.assertEqual(result['draw_commands_with_unknown_pipeline'], 5)
        self.assertEqual(len(result['passes']), 1)

    def test_secondary_execution_occurrences_remain_separate(self):
        data = sample(); data['submissions'][0]['commands'] *= 2
        result = frame_trace.summarize(data)
        self.assertEqual(result['image_attachment_scopes']['10'], [0, 1])
        self.assertEqual(result['draw_command_counts']['indexed'], 8)

    def test_request_requires_active_diagnostic_and_stops_at_four(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); path = root / 'capture.json'
            path.write_text(json.dumps({'phase': 'launching', 'frame_trace_requested': False}))
            with self.assertRaises(ValueError): frame_trace.request(root)
            path.write_text(json.dumps({'phase': 'launching', 'frame_trace_requested': True}))
            for sequence in range(1, 5):
                self.assertEqual(frame_trace.request(root), sequence)
                self.assertEqual(int((root / 'frame.request').read_text()), sequence)
            with self.assertRaises(ValueError): frame_trace.request(root)
            self.assertEqual(list(root.glob('.frame-request-*')), [])

    def test_parser_rejects_draw_shape_and_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame.json'; data = sample()
            path.write_text(json.dumps(data)); self.assertEqual(frame_trace.read_frame(path)['request_sequence'], 1)
            data['submissions'][0]['commands'][0]['operations'][0]['draws'][0]['counts'] = [-1] * 8
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError): frame_trace.read_frame(path)
            path.write_bytes(b' ' * (frame_trace.MAX_BYTES + 1))
            with self.assertRaises(ValueError): frame_trace.read_frame(path)

    def test_fingerprint_correlates_only_matching_size_and_bytes(self):
        self.assertEqual(frame_trace.fnv1a64(b'hello'), 'a430d84680aabd0b')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); code = b'original fixture bytes'
            (root / '0000000000000001.spv').write_bytes(code)
            (root / '0000000000000002.spv').write_bytes(b'x' * len(code))
            data = sample(); data['pipelines'][0]['shaders'] = [{'known': True, 'fnv1a64': frame_trace.fnv1a64(code), 'code_bytes': len(code)}]
            result = frame_trace.correlate(data, root)
            self.assertEqual(result['shaders'][0]['candidates'][0]['catalog_id'], '0000000000000001')
            self.assertFalse(result['visible_scene_role_proven'])

    def test_cache_identifiers_require_same_implementation_algorithm(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'identifiers.json'
            data = sample(); data['shader_identifier_algorithm'] = 'ab' * 16
            data['pipelines'][0]['shaders'] = [{'known': False, 'module_identifier': '1234abcd'}]
            catalog = {'schema_version': 1, 'shader_identifier_algorithm': 'cd' * 16,
                       'shaders': [{'catalog_id': '0000000000000001', 'module_identifier': '1234abcd', 'fnv1a64': '0' * 16, 'code_bytes': 20}]}
            path.write_text(json.dumps(catalog))
            with self.assertRaises(ValueError): frame_trace.correlate(data, None, identifier_catalog=path)
            catalog['shader_identifier_algorithm'] = data['shader_identifier_algorithm']; path.write_text(json.dumps(catalog))
            result = frame_trace.correlate(data, None, identifier_catalog=path)
            self.assertEqual(result['shaders'][0]['candidates'][0]['catalog_id'], '0000000000000001')
            self.assertEqual(result['module_identifiers_observed'], 1)

    def test_missing_shader_code_is_explicit(self):
        data = sample(); data['pipelines'][0]['shaders'] = [{'known': False}, {'known': False, 'module_identifier': 'aabb'}]
        result = frame_trace.summarize(data)
        self.assertEqual(result['stages_with_code_fingerprint'], 0)
        self.assertEqual(result['stages_with_module_identifier'], 1)
        self.assertEqual(result['stages_without_code_or_identifier'], 1)

    def test_vertex_candidates_are_associated_with_attachment_scopes(self):
        data = sample(); data['pipelines'][0]['shaders'] = [{'stage': 1, 'known': False, 'module_identifier': 'aabb'}]
        candidate = {'catalog_id': '0' * 16, 'static_final_clip_matrix_found': True, 'static_input_matrix_chain_found': True}
        correlation = {'shaders': [{'module_identifier': 'aabb', 'candidates': [candidate]}]}
        result = frame_trace.summarize(data, correlation)
        self.assertEqual(result['pipeline_associations'][0]['vertex_candidates'], [candidate])
        self.assertEqual(result['passes'][0]['draw_commands_with_static_projection_candidate'], 5)
        self.assertEqual(result['passes'][0]['scene_role'], 'unclassified')
        self.assertFalse(result['scene_pass_identified'])


if __name__ == '__main__': unittest.main()
