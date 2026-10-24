import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import vulkan_trace


class TraceReports(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()
