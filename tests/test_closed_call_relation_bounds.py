"""Independent byte-budget controls for the new CPU report, without a runtime."""
import copy
import unittest
from unittest.mock import patch

from tools import closed_call_relation_validate as relation
from test_closed_call_relation_validate import inputs, lifecycle, foreign_open
from test_original_call_owner_validate import rows


class ClosedCallRelationBudgetTests(unittest.TestCase):
    def test_route_scalar_is_unknown_when_recording_faults(self):
        for flag in ('faulted', 'transport_overflow'):
            values = rows()
            values[-1][flag] = True
            values[-1]['coverage_complete'] = False
            report = relation.analyze_closed_call_relation(**inputs(values))
            self.assertFalse(report['closed_selected_original_call_relation_known'])
            self.assertFalse(report['scope_status'][0]['guaranteed_route_peaks_known'])
            self.assertIsNone(report['scope_status'][0]['guaranteed_instance_route_peak_lower_bound'])

    def test_route_peak_diagnostic_is_unknown_with_a_pending_lifecycle(self):
        values = foreign_open()
        extra = next(row for row in values if row.get('kind') == 'create_instance'
                     and row['instance_generation'] == 2)
        extra.update(original_returned=False, return_ordinal=0, result=None, raw_instance=0)
        extra['instance_debug_utils']['enabled_on_success'] = False
        values[-1].update(transition_count=23, active_instances=0)
        report = relation.analyze_closed_call_relation(**inputs(values))
        self.assertFalse(report['closed_selected_original_call_relation_known'])
        scope = report['scope_status'][0]
        self.assertFalse(scope['guaranteed_route_peaks_known'])
        self.assertIsNone(scope['guaranteed_instance_route_peak_lower_bound'])
        self.assertIsNone(scope['guaranteed_device_route_peak_lower_bound'])

    def test_certain_route_overlap_refuses_but_destroy_return_overlap_is_not_invented(self):
        for destroy_entry, create_entry, create_return, expected in (
                (89, 87, 88, False),  # All 33 creations returned before any destroy entry.
                (87, 88, 89, True)):  # Removal may precede the delayed destroy AFTER90.
            values = rows()
            extra = []
            for index in range(33):
                created = copy.deepcopy(lifecycle(values, 'create_instance'))
                created.update(instance_generation=index + 2, raw_instance=1000 + index,
                               output_pointer=9000 + index, operation_ordinal=23 + index * 2,
                               return_ordinal=24 + index * 2)
                if index == 32:
                    created.update(operation_ordinal=create_entry, return_ordinal=create_return)
                extra.append(created)
            destroyed = copy.deepcopy(lifecycle(values, 'destroy_instance'))
            destroyed.update(instance_generation=2, raw_instance=1000,
                             operation_ordinal=destroy_entry, return_ordinal=90)
            extra.append(destroyed)
            extra.sort(key=lambda row: row['operation_ordinal'])
            first_owner = next(i for i, row in enumerate(values)
                               if row['event'] == 'original_call_owner')
            values[first_owner:first_owner] = extra
            values[-1].update(lifecycle_calls_seen=39, lifecycle_retained=39,
                              transition_count=90, active_instances=32,
                              coverage_complete=False)
            with self.subTest(destroy_entry=destroy_entry):
                report = relation.analyze_closed_call_relation(**inputs(values))
                self.assertIs(report['closed_selected_original_call_relation_known'], expected)
                self.assertEqual(report['scope_status'][0]['guaranteed_instance_route_peak_lower_bound'],
                                 32 if expected else 33)
                self.assertFalse(report['all_saved_scopes_lifecycle_closed'])

    def test_known_open_instances_still_obey_the_original_route_capacity(self):
        # All calls return, all generations differ and the common clock is
        # coherent. The real observer still has only 32 live instance routes.
        for count, expected in ((32, True), (33, False)):
            values = rows()
            original = lifecycle(values, 'create_instance')
            extra = []
            for index in range(count):
                created = copy.deepcopy(original)
                created.update(instance_generation=index + 2,
                               raw_instance=1000 + index, output_pointer=9000 + index,
                               operation_ordinal=23 + index * 2,
                               return_ordinal=24 + index * 2)
                extra.append(created)
            first_owner = next(i for i, row in enumerate(values)
                               if row['event'] == 'original_call_owner')
            values[first_owner:first_owner] = extra
            values[-1].update(lifecycle_calls_seen=5 + count, lifecycle_retained=5 + count,
                              transition_count=22 + count * 2, active_instances=count,
                              coverage_complete=False)
            with self.subTest(active_instances=count):
                report = relation.analyze_closed_call_relation(**inputs(values))
                self.assertIs(report['closed_selected_original_call_relation_known'], expected)
                self.assertFalse(report['legacy_strict_result']['original_call_owner_known'])
                self.assertFalse(report['all_saved_scopes_lifecycle_closed'])

    def test_ascii_wire_bytes_have_an_exact_independent_boundary(self):
        # The literal expected wire representation is 18 ASCII bytes. The
        # Unicode character is not counted as its Python character length.
        value = {'value': '\u00e9'}
        wire = b'{"value":"\\u00e9"}'
        before = copy.deepcopy(value)
        with patch.object(relation, 'MAX_REPORT_BYTES', len(wire)):
            self.assertIs(relation._bounded_report(value), value)
        with patch.object(relation, 'MAX_REPORT_BYTES', len(wire) - 1):
            with self.assertRaises(ValueError):
                relation._bounded_report(value)
        self.assertEqual(value, before, 'A refused report is never truncated in place')

    def test_whole_public_report_is_checked_and_input_remains_unchanged(self):
        args = inputs()
        before = copy.deepcopy(args)
        with patch.object(relation, 'MAX_REPORT_BYTES', 64):
            with self.assertRaises(ValueError):
                relation.analyze_closed_call_relation(**args)
        self.assertEqual(args, before)
        report = relation.analyze_closed_call_relation(**args)
        self.assertTrue(report['closed_selected_original_call_relation_known'])
        self.assertEqual(report['serialization_limit_bytes'], 32 * 1024 * 1024)
        self.assertFalse(report['report_truncated'])

    def test_nonfinite_json_never_produces_a_partial_report(self):
        value = {'retained': [1, 2, 3], 'not_a_wire_number': float('inf')}
        with self.assertRaises(ValueError):
            relation._bounded_report(value)
        self.assertEqual(value['retained'], [1, 2, 3])


if __name__ == '__main__':
    unittest.main()
