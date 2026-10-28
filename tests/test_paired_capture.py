import contextlib
import io
import json
from pathlib import Path
import signal
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import paired_capture
import vulkan_trace
import probe


class PairedCaptureTests(unittest.TestCase):
    def test_launch_argument_and_cache_pin_only_change_private_text(self):
        game=Path('/prefix/drive_c/Program Files (x86)/Diablo IV');out=Path('/private/session')
        original='export PROTONPATH="GE-Proton"\nexport WINEPREFIX="/prefix"\nexport __GL_SHADER_DISK_CACHE_PATH="/original"\nexport DXVK_STATE_CACHE_PATH="/original"\n'+ \
            '/usr/bin/umu-run '+__import__('shlex').quote(str(game.parent/'Battle.net'/'Battle.net Launcher.exe'))+'\n'
        result=paired_capture.launcher_source(original,'/runtime with spaces',game,out,True)
        import shlex
        self.assertEqual(shlex.split(result.splitlines()[-1])[-1],'--exec=launch Fen')
        self.assertEqual(Path(shlex.split(result.splitlines()[-1])[1]).name,'Battle.net.exe')
        self.assertIn("export PROTONPATH='/runtime with spaces'",result)
        self.assertEqual(result.count(str(out/'vendor-cache')),2)
        self.assertNotIn('--exec',original)
        with self.assertRaises(ValueError):paired_capture.launcher_source(original+'unexpected\n','/runtime',game,out,True)
        with self.assertRaises(ValueError):paired_capture.launcher_source(original.replace('export WINEPREFIX="/prefix"\n',''),'/runtime',game,out,True)
        with self.assertRaises(ValueError):paired_capture.launcher_source(original.replace('WINEPREFIX="/prefix"','WINEPREFIX="/other"'),'/runtime',game,out,True)

    def test_layer_fingerprint_and_create_only_evidence_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/vulkan_trace.LIBRARY).write_bytes(b'fixture')
            (root/vulkan_trace.MANIFEST).write_text(json.dumps({'layer':{'name':vulkan_trace.LAYER,'library_path':'./'+vulkan_trace.LIBRARY}}))
            manifest={'purpose':'vulkan_trace_diagnostic','layer_sha256':probe.digest(root/vulkan_trace.LIBRARY),
                      'manifest_sha256':probe.digest(root/vulkan_trace.MANIFEST),'validation':{'paired_pipeline_mode':'create_only'}}
            for key in ('paired_pipeline_fixture_passed','forwarding_tests_passed','observer_pixels_unchanged','geometry_parallax_unchanged',
                        'loaded_trace_verified','steam_runtime_load_passed','frame_trace_fixture_passed'):manifest['validation'][key]=True
            manifest['validation']['layer_sha256']=manifest['layer_sha256']
            (root/'trace-bundle.json').write_text(json.dumps(manifest));self.assertEqual(paired_capture.validate_layer(root),manifest)
            (root/vulkan_trace.LIBRARY).write_bytes(b'changed')
            with self.assertRaises(ValueError):paired_capture.validate_layer(root)

    def test_cleanup_refuses_foreign_markers_and_recycled_processes(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for pid,marker,start in ((57,'/owned','42'),(58,'/foreign','41'),(59,'/owned','41')):
                p=root/str(pid);p.mkdir();(p/'cmdline').write_bytes(b'/game/Diablo IV.exe\0')
                (p/'environ').write_bytes(('SECRET=do-not-print\0D4VR_TRACE_DIR='+marker+'\0').encode())
                (p/'stat').write_text(str(pid)+' (Diablo IV.exe) S '+'0 '*18+start+'\n')
            stdout=io.StringIO()
            with patch('pathlib.Path',return_value=root),patch.object(sys,'argv',['script','/owned',json.dumps({'57':'41','58':'41','59':'41'})]), \
                    patch('os.pidfd_open',side_effect=lambda pid:pid+1000,create=True) as opened,patch('os.close'), \
                    patch('signal.pidfd_send_signal',create=True) as sent,contextlib.redirect_stdout(stdout):
                exec(paired_capture.OWNED_SCRIPT,{})
            self.assertEqual(opened.call_args_list[0].args,(59,));sent.assert_called_once_with(1059,signal.SIGTERM)
            rows=json.loads(stdout.getvalue());self.assertEqual({r['pid'] for r in rows},{57,59})
            self.assertNotIn('SECRET',stdout.getvalue())

    def test_original_eye_override_is_purged_from_launch_environment(self):
        capture={'variables':{'D4VR_PAIR_LEFT_FILE':'/left','D4VR_PAIR_RIGHT_FILE':'/right'},'private_launcher':'/launch'}
        with patch.object(paired_capture,'host_command',return_value=[]):command=paired_capture.command(capture)
        self.assertIn('D4VR_PAIR_LEFT_FILE=/left',command)
        self.assertIn('D4VR_EYE_STATE_FILE',command)
        self.assertNotIn('D4VR_EYE_STATE_FILE=',command)
        self.assertEqual(command[-2:],['/usr/bin/bash','/launch'])

    def test_inactive_telemetry_does_not_claim_pair_drawing(self):
        rows={'records':[{'rows':[[1,2,3,4],[1,2,3,4],[1,1,1,1],[0,0,0,1]]}],
              'incomplete_or_duplicate_groups':0,'rejected_rows':0}
        with patch.object(paired_capture.gpu_messages,'read_records',return_value=rows):result=paired_capture.inactive_telemetry(Path('log'))
        self.assertEqual(result['unchanged_inactive_samples'],1)
        self.assertEqual(result['game_images_submitted_to_steamvr'],0)

    def test_helper_trace_cannot_confirm_a_diablo_device(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'process-observations.json').write_text(json.dumps({'observations':[
                {'processes':[{'pid':7,'names':['diablo iv.exe']}]}]}))
            helper={'trace_loaded':True,'process_traces':[{'file':'vulkan-8.jsonl','devices':[{'paired_pipelines':{'executable_pairs':99},'clean_destroy':True}]}]}
            with patch.object(paired_capture.vulkan_trace,'report',return_value=helper):result=paired_capture.session_report(root)
            self.assertTrue(result['any_layer_trace_loaded']);self.assertTrue(result['game_process_observed'])
            self.assertFalse(result['game_device_trace_found']);self.assertFalse(result['game_pair_creation_confirmed'])
            helper['process_traces'][0]['file']='vulkan-7.jsonl';helper['process_traces'][0]['devices'][0]['clean_destroy']=False
            with patch.object(paired_capture.vulkan_trace,'report',return_value=helper):result=paired_capture.session_report(root)
            self.assertTrue(result['game_pair_creation_confirmed']);self.assertTrue(result['snapshot_counts_may_be_partial'])
            self.assertFalse(result['private_pipelines_drawn']);self.assertFalse(result['game_stereo_submitted'])

    @unittest.skipUnless(sys.platform=='linux','Linux process launcher')
    def test_prepare_freezes_only_loader_files_without_starting_or_mutating_originals(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);prefix=root/'prefix';game=prefix/'drive_c'/'Program Files (x86)'/'Diablo IV';game.mkdir(parents=True)
            shader=root/'shaders';shader.mkdir();(shader/'bundle.json').write_text('{}')
            layer=root/'validated';layer.mkdir()
            for name in (vulkan_trace.LIBRARY,vulkan_trace.MANIFEST):(layer/name).write_bytes(b'tested')
            manifest={'layer_sha256':probe.digest(layer/vulkan_trace.LIBRARY),'manifest_sha256':probe.digest(layer/vulkan_trace.MANIFEST)}
            bundle={'purpose':'eye_transform_diagnostic','eye_state_abi':1,'eye_state_specialization_id':4,'stereo_telemetry':True,
                    'gpu_validation':{'stereo_gpu_fixture_passed':True},'game_version':'test','proton_path':'/tested-runtime','entries':[{}]}
            import shlex
            original='export PROTONPATH="GE-Proton"\nexport WINEPREFIX='+shlex.quote(str(prefix))+'\n/usr/bin/umu-run '+shlex.quote(str(game.parent/'Battle.net'/'Battle.net Launcher.exe'))+'\n'
            launcher=root/'launcher.sh';launcher.write_text(original);output=root/'capture'
            with patch.object(paired_capture,'validate_layer',return_value=manifest),patch.object(paired_capture.camera_capture,'validate_bundle',return_value=bundle), \
                    patch.object(paired_capture.probe,'profile',return_value={'game_version':'test'}),patch.object(paired_capture.probe,'check_target',return_value={'original_library_matches':True}), \
                    patch.object(paired_capture.shader_capture,'launcher_processes',return_value={}),patch.object(paired_capture.subprocess,'Popen') as popen:
                capture=paired_capture.prepare(game,launcher,shader,layer,output,True)
            popen.assert_not_called();self.assertEqual(launcher.read_text(),original)
            self.assertEqual(list((output/'layer').glob('*.json')),[output/'layer'/vulkan_trace.MANIFEST])
            self.assertEqual(capture['variables']['VK_ADD_LAYER_PATH'],str(output/'layer'))
            self.assertEqual(capture['phase'],'prepared');self.assertFalse(capture['private_pipelines_drawn'])
            self.assertNotIn('D4VR_EYE_STATE_FILE',capture['variables'])
            self.assertEqual((output/'left.bin').stat().st_size,176)


if __name__=='__main__':unittest.main()
