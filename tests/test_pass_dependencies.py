import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import pass_dependencies


def sample():
    view={'image_id':1,'view_id':2,'image_known':True,'view_known':True,'base_mip':0,'mip_count':1,'base_layer':0,'layer_count':1}
    def op(load):
        return {'kind':'dynamic_rendering','truncated':False,'area':[0,0,64,64],
                'colors':[{'view':copy.deepcopy(view),'load':load,'store':0,'resolve_mode':0,'clear_value_words':[0,0,0,0] if load==1 else None}],
                'depth':None,'stencil':None,'draws':[]}
    return {'schema_version':1,'scope':'single_submission','request_sequence':1,'present_before':0,'present_after':0,'truncated':False,'pipelines':[],
            'submissions':[{'complete':True,'result':0,'queue_family':0,'queue_index':0,
                            'commands':[{'id':1,'generation':0,'missing':False,'truncated':False,'secondary_depth':0,'operations':[op(1),op(0),op(0)]}]}]}


class Dependencies(unittest.TestCase):
    def test_closure_includes_intervening_writers_and_never_authorizes_replay(self):
        r=pass_dependencies.audit(sample(),[2]);self.assertEqual(r['required_attachment_scope_candidates'],[0,1,2])
        self.assertEqual(r['possible_clear_scopes'],[0]);self.assertEqual(r['loads_without_observed_clear'],0)
        self.assertEqual(r['clear_values_missing'],0);self.assertFalse(r['replay_allowed']);self.assertFalse(r['gpu_execution_order_known'])

    def test_older_captures_retain_missing_clear_values(self):
        d=sample();del d['submissions'][0]['commands'][0]['operations'][0]['colors'][0]['clear_value_words']
        self.assertGreater(pass_dependencies.audit(d,[2])['clear_values_missing'],0)

    def test_partial_clear_or_discard_does_not_establish_full_initialization(self):
        for partial in (True,False):
            d=sample();ops=d['submissions'][0]['commands'][0]['operations']
            if partial:ops[0]['area']=[0,0,32,32]
            else:ops[1]['colors'][0]['store']=1
            self.assertEqual(pass_dependencies.audit(d,[2])['loads_without_observed_clear'],1)

    def test_unknown_identity_and_different_view_do_not_match(self):
        for unknown in (True,False):
            d=sample();view=d['submissions'][0]['commands'][0]['operations'][2]['colors'][0]['view']
            if unknown:view['image_id']=0
            else:view['view_id']=3
            self.assertEqual(pass_dependencies.audit(d,[2])['loads_without_observed_clear'],1)

    def test_cross_queue_and_secondary_order_are_not_inferred(self):
        for secondary in (False,True):
            d=sample();s=d['submissions'][0];new=copy.deepcopy(s);new['commands'][0]['operations']=s['commands'][0]['operations'][1:]
            s['commands'][0]['operations']=s['commands'][0]['operations'][:1]
            if secondary:new['commands'][0]['secondary_depth']=1
            else:new['queue_index']=1
            d['submissions'].append(new)
            self.assertEqual(pass_dependencies.audit(d,[2])['loads_without_observed_clear'],1)
        # An intervening scope on another queue/secondary also prevents a chain,
        # even when the original clear and final consumer share a primary queue.
        for secondary in (False,True):
            d=sample();s=d['submissions'][0];operations=s['commands'][0]['operations']
            submissions=[]
            for index,operation in enumerate(operations):
                part=copy.deepcopy(s);part['commands'][0]['operations']=[operation]
                if index==1:
                    if secondary:part['commands'][0]['secondary_depth']=1
                    else:part['queue_index']=1
                submissions.append(part)
            d['submissions']=submissions
            result=pass_dependencies.audit(d,[2]);self.assertEqual(result['loads_without_observed_clear'],1)
            self.assertEqual(result['load_dependencies'][0]['unresolved_reason'],'unknown_or_cross_queue_or_secondary_order')

    def test_upstream_other_attachment_load_remains_unresolved(self):
        d=sample();ops=d['submissions'][0]['commands'][0]['operations'];unknown=copy.deepcopy(ops[0]['colors'][0])
        unknown.update(load=0,clear_value_words=None);unknown['view']['image_id']=99
        ops[0]['colors'].append(unknown)
        r=pass_dependencies.audit(d,[2]);self.assertEqual(r['loads_without_observed_clear'],1)
        self.assertIn(0,r['required_attachment_scope_candidates']);self.assertFalse(r['replay_allowed'])

    def test_failed_submissions_and_scope_budget_are_explicit(self):
        d=sample();failed=copy.deepcopy(d['submissions'][0]);failed['result']=-4;d['submissions'].append(failed)
        self.assertTrue(pass_dependencies.audit(d,[2])['metadata_incomplete'])
        for scopes in ([],[99],[1,1]):
            with self.assertRaises(ValueError):pass_dependencies.audit(d,scopes)
