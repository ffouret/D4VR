import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import vulkan_trace


class TraceReports(unittest.TestCase):
    def test_eye_bridge_stats_are_distinct_from_stereo_rendering(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory,[{'event':'eye_control_error','device':1,'reason':'feature unavailable'},
                {'event':'device','device':1},{'event':'eye_control','device':1,'control':{'prepared_stage_overrides':0}},
                {'event':'eye_control_pipeline','device':1,'control':{'prepared_stage_overrides':2}}])
            report = vulkan_trace.read_trace(Path(directory)/'vulkan-123.jsonl')
            self.assertEqual(report['devices'][0]['eye_control']['prepared_stage_overrides'],2)
            self.assertEqual(len(report['eye_control_errors']),1)
            self.assertFalse(report['game_stereo_submitted'])

    def write(self, directory, rows):
        path = Path(directory) / 'vulkan-123.jsonl'
        path.write_text(''.join(json.dumps(dict(schema_version=1, **row)) + '\n' for row in rows))
        return path

    def test_final_totals_override_sparse_present_samples(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, [
                {'event': 'instance', 'requested_api': '1.3.0'},
                {'event': 'device', 'device': 1},
                {'event': 'present', 'device': 1, 'present_call': 600, 'total_commands': {'execute_generated_ext': 10}},
                {'event': 'device_destroy', 'device': 1, 'present_calls': 605, 'submissions': 900, 'commands': {'execute_generated_ext': 15}},
            ])
            report = vulkan_trace.report(Path(directory))
            device = report['process_traces'][0]['devices'][0]
            self.assertTrue(device['clean_destroy'])
            self.assertEqual(device['last_total_commands']['execute_generated_ext'], 15)
            self.assertEqual(device['last_present_call'], 605)
            self.assertFalse(report['game_stereo_submitted'])

    def test_abrupt_exit_retains_partial_status(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, [
                {'event': 'device', 'device': 1},
                {'event': 'present', 'device': 1, 'present_call': 600, 'total_commands': {'draw': 100}},
            ])
            device = vulkan_trace.report(Path(directory))['process_traces'][0]['devices'][0]
            self.assertFalse(device['clean_destroy'])
            self.assertEqual(device['last_total_commands']['draw'], 100)

    def test_orphan_and_duplicate_device_rejected(self):
        for rows in ([{'event': 'submit', 'device': 2}],
                     [{'event': 'device', 'device': 1}, {'event': 'device', 'device': 1}]):
            with tempfile.TemporaryDirectory() as directory:
                path = self.write(directory, rows)
                with self.assertRaises(ValueError):
                    vulkan_trace.read_trace(path)

    def test_missing_trace_is_not_reported_as_loaded(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertFalse(vulkan_trace.report(Path(directory))['trace_loaded'])

    def test_record_budget_and_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.write(directory, [{'event': 'instance'}] * 513)
            with self.assertRaises(ValueError):
                vulkan_trace.read_trace(path)
            link = Path(directory) / 'link.jsonl'
            try:
                link.symlink_to(path)
            except OSError:
                self.skipTest('Symlink creation unavailable on this host')
            with self.assertRaises(ValueError):
                vulkan_trace.read_trace(link)

    def test_extension_names_from_different_instances_are_not_combined(self):
        trace = {'process_traces': [{'file': 'vulkan-1.jsonl',
                  'instances': [{'requested_api': '1.2.123', 'extensions': ['VK_A']},
                                {'requested_api': '1.3.0', 'extensions': ['VK_B']}],
                  'devices': [{'device': 1, 'gpu': 'fixture', 'extensions': ['VK_D']}]}]}
        requirements = {'binding': 'XR_KHR_vulkan_enable', 'minimum_api': '1.0.0', 'maximum_tested_api': '1.2.0',
                        'required_instance_extensions': ['VK_A', 'VK_B'], 'required_device_extensions': ['VK_D', 'VK_E']}
        result = vulkan_trace.compare_requirements(trace, requirements)
        process = result['processes'][0]
        self.assertEqual(process['instances'][0]['missing_literal_extension_names'], ['VK_B'])
        self.assertEqual(process['instances'][1]['missing_literal_extension_names'], ['VK_A'])
        self.assertTrue(process['instances'][0]['within_reported_api_range'])  # Patch is ignored by this XR contract.
        self.assertFalse(process['instances'][1]['within_reported_api_range'])
        self.assertEqual(process['devices'][0]['missing_literal_extension_names'], ['VK_E'])
        self.assertFalse(result['instance_device_association_known'])
        self.assertFalse(result['game_device_binding_proven'])

    def test_borrowed_binding_probe_does_not_prove_stereo(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, [{'event': 'device', 'device': 1},
                                   {'event': 'xr_binding', 'device': 1, 'binding': {'success': True, 'swapchain_images': 3, 'frames_submitted': 0}}])
            result = vulkan_trace.report(Path(directory))
            self.assertTrue(result['process_traces'][0]['devices'][0]['xr_binding_probe']['success'])
            self.assertFalse(result['game_stereo_submitted'])


if __name__ == '__main__':
    unittest.main()
