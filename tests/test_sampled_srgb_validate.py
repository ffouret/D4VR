import argparse
import copy
from decimal import Decimal, localcontext
import json
import math
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import sampled_srgb_validate as s


def decimal_transfer(v, encode):
    with localcontext() as ctx:
        ctx.prec=60; x=Decimal(str(v))
        if encode:
            return Decimal('12.92')*x if x<=Decimal('.0031308') else Decimal('1.055')*x**(Decimal(1)/Decimal('2.4'))-Decimal('.055')
        return x/Decimal('12.92') if x<=Decimal('.04045') else ((x+Decimal('.055'))/Decimal('1.055'))**Decimal('2.4')


class TransferTests(unittest.TestCase):
    def test_decimal_reference_all_codes_and_branches(self):
        for encode in (False,True):
            for x in [c/255 for c in range(256)]+[.0031308,.04045]:
                ideal=float(decimal_transfer(x,encode)); lo,hi=s.transfer_interval((x,x),encode)
                self.assertLessEqual(lo,ideal);self.assertGreaterEqual(hi,ideal)
                self.assertLess(hi-lo,.0002)  # proves the policy is discriminating
    def test_floor_and_ceil_are_both_admitted(self):
        self.assertEqual(s.quantize_interval((.5,.5)),(127,128))
        self.assertEqual(s.quantize_interval((0,0)),(0,0));self.assertEqual(s.quantize_interval((1,1)),(255,255))
        self.assertEqual(s.quantize_interval((-2,3)),(0,255))
    def test_known_transfer_values_and_alpha_exception(self):
        self.assertAlmostEqual(s.srgb_encode(.5),.7353569830524495)
        self.assertAlmostEqual(s.srgb_decode(.5),.21404114048223255)
        self.assertEqual(s.source_bounds((128/255,128/255),True,True),(128,128))
        self.assertGreater(s.source_bounds((128/255,128/255),True)[0],180)
        self.assertEqual(s.blit_bounds(128,44),(128,128));self.assertEqual(s.blit_bounds(128,50,True),(128,128))
        self.assertLess(s.blit_bounds(128,50)[1],57)
    def test_nonfinite_and_reversed_inputs_refused(self):
        for x in (math.inf,-math.inf,math.nan,True):
            for f in (s.srgb_encode,s.srgb_decode,s.encoded_interval):
                with self.assertRaises(ValueError):f(x)
        for interval in ((.5,.4),(-.1,.5),(.5,1.1)):
            with self.assertRaises(ValueError):s.transfer_interval(interval)
        with self.assertRaises(ValueError):s.quantize_interval((.8,.2))
        with self.assertRaises(ValueError):s.blit_bounds(True,44)
    def test_raw_load_copy_and_source_image_format(self):
        for code in (3,5,7):
            self.assertEqual(s.blit_bounds(code,44),(code,code))
            self.assertLessEqual(s.blit_bounds(code,50)[1],1)
        # View50 is deliberately absent from this BLIT API.
        self.assertNotEqual(s.blit_bounds(128,44),s.blit_bounds(128,50))
    def test_closed_policy_not_a_normative_bound(self):
        self.assertFalse(s.POLICY['normative_precision_upper_bound_claimed'])
        self.assertFalse(s.POLICY['adaptive_widening_allowed'])
        self.assertEqual(s.POLICY['version'],1)
    def test_original_assets_frozen_new_rectangle_distinct(self):
        self.assertEqual(s.texture_bytes(0),s.base.texture_bytes(0))
        self.assertEqual(s.texture_bytes(1),s.base.texture_bytes(1))
        rect=s.texture_bytes(1,True)
        self.assertEqual(rect[2],31);self.assertEqual(rect[3],83)
        self.assertEqual(rect[(2*8+2)*4+2],169)
        self.assertGreater(len(set(rect[3::4])),8)


class FinitePixelTests(unittest.TestCase):
    def setUp(self):
        self.patches=[patch.object(s,'WIDTH',16),patch.object(s,'HEIGHT',16)]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
    def images(self,name,mode,wrong=None):
        # This generator evaluates the ideal transfer, independently of the
        # interval oracle; nearest is ONE permitted fixed-point rounding.
        st,_,_=s.state(name); original=bytearray();diagnostic=bytearray()
        for y in range(16):
            for x in range(16):
                if not s.occupied(x,y,st['viewport'],st['scissor']):
                    original.extend(s.SENTINEL);diagnostic.extend(s.SENTINEL);continue
                tex=s.texture_pixel(x,y,st);u,v=s.uv_at(x,y,st['viewport'],st['shift_texels'])
                for output,diag in ((original,False),(diagnostic,True)):
                    pixel=[]
                    for channel,code in enumerate(tex):
                        f=code/255
                        if diag and channel in (1,2):
                            t=u if channel==2 else v
                            f=(1+253*((t+1)/3))/255
                        if mode!='unorm' and channel!=3:f=s.srgb_encode(f)
                        pixel.append(round(f*255))
                    output.extend(pixel)
        dest=[]
        for source in (original,diagnostic):
            dest.append(bytearray(round(s.srgb_decode(c/255)*255) if mode=='srgb' and i%4!=3 else c for i,c in enumerate(source)))
        return original,diagnostic,dest[0],dest[1]
    def test_all_six_cases_three_modes(self):
        for mode in s.MODES:
            for name in s.NAMES:
                r=s.audit_pixels(*self.images(name,mode),name,mode)
                self.assertTrue(all(v==0 for v in r['errors'].values()))
    def test_negative_height_different_v_direction(self):
        normal=s.state('B_repeat_offset')[0];reverse=s.state('negative_height')[0]
        self.assertGreater(s.uv_at(0,0,normal['viewport'],[0,0])[1],.9)
        self.assertLess(s.uv_at(0,0,reverse['viewport'],[0,0])[1],.1)
    def test_absent_double_and_swapped_transfer_rejected(self):
        for mistake in ('absent','double','swap'):
            a,b,c,d=self.images('A_clamp_zero','mutable-srgb')
            for i in range(0,len(b),4):
                for channel in (1,2):
                    if mistake=='absent':b[i+channel]=round(s.srgb_decode(b[i+channel]/255)*255)
                    if mistake=='double':b[i+channel]=round(s.srgb_encode(b[i+channel]/255)*255)
                if mistake=='swap':b[i+1],b[i+2]=b[i+2],b[i+1]
            d[:]=b
            with self.assertRaises(ValueError):s.audit_pixels(a,b,c,d,'A_clamp_zero','mutable-srgb')
    def test_source_vs_view_format_confusion_rejected(self):
        for mode in ('mutable-srgb','srgb'):
            a,b,c,d=self.images('B_repeat_offset',mode)
            for src,dst in ((a,c),(b,d)):
                dst[:]=bytes(src) if mode=='srgb' else bytes(round(s.srgb_decode(v/255)*255) if i%4!=3 else v for i,v in enumerate(src))
            with self.assertRaises(ValueError):s.audit_pixels(a,b,c,d,'B_repeat_offset',mode)
    def test_absolute_cpu_oracle_required_in_addition_to_relation(self):
        a,b,c,d=self.images('A_clamp_zero','srgb')
        a[:]=bytes(len(a));b[:]=bytes(len(b));c[:]=bytes(len(c));d[:]=bytes(len(d))
        with self.assertRaisesRegex(ValueError,'absolute_original_source'):s.audit_pixels(a,b,c,d,'A_clamp_zero','srgb')
    def test_load_outside_not_encoded_at_attachment_but_decoded_at_blit(self):
        for mode in s.MODES:
            name='B_clamp_offset_center_scissor_LOAD';a,b,c,d=self.images(name,mode)
            self.assertEqual(a[:4],s.SENTINEL)
            self.assertEqual(c[:4],s.SENTINEL if mode!='srgb' else bytes((1,0,0,255)))
            self.assertEqual(s.audit_pixels(a,b,c,d,name,mode)['unchanged_source_load_pixels'],192)
            b[2]=100;d[2]=100
            with self.assertRaises(ValueError):s.audit_pixels(a,b,c,d,name,mode)
    def test_blue_and_nonopaque_alpha_remain_original_dependencies(self):
        name='B_rectangle_input'
        for channel in (0,3):
            a,b,c,d=self.images(name,'mutable-srgb');b[channel]^=1;d[channel]^=1
            with self.assertRaisesRegex(ValueError,'source_ba'):s.audit_pixels(a,b,c,d,name,'mutable-srgb')
        a,b,c,d=self.images(name,'srgb');r=s.audit_pixels(a,b,c,d,name,'srgb')
        self.assertGreater(r['fixed_witness_rejections']['alpha_transfer'],0)
        self.assertEqual(a[0::4],b[0::4]);self.assertEqual(a[3::4],b[3::4])
    def test_changed_sampling_uv_not_replaced_by_regular_rg(self):
        a,b,c,d=self.images('B_rectangle_input','unorm')
        b[0::4]=bytes([31])*256;d[0::4]=b[0::4]
        with self.assertRaises(ValueError):s.audit_pixels(a,b,c,d,'B_rectangle_input','unorm')


class MetadataTests(unittest.TestCase):
    def queries(self):
        q=dict(completed_before_resource_creation=True,image_format_list_core_version=4202496,
               format_features=[dict(format=f,linear=0,optimal=n,buffer=0) for f,n in ((37,0x8001),(44,0xcc80),(50,0xc480))],images=[])
        for f,u,flags,formats in ((44,19,0,[]),(44,19,8,[44,50]),(50,19,0,[]),(44,3,0,[]),(37,6,0,[])):
            q['images'].append(dict(format=f,type=1,tiling=0,usage=u,flags=flags,format_list=formats,result=0,
                extent=[8,8,1] if f==37 else [2560,1440,1],max_extent=[8192,8192,1],max_mip_levels=1,max_array_layers=1,sample_counts=1,max_resource_size=1<<30))
        return q
    def test_exact_five_preflight_queries(self):
        q=self.queries()
        for mode in s.MODES:s.audit_queries(q,mode)
        changes=[lambda q:q['images'].pop(),lambda q:q['images'][1].update(flags=0),
            lambda q:q['images'][1].update(format_list=[50]),lambda q:q['images'][2].update(result=-11),
            lambda q:q['images'][4].update(usage=2),lambda q:q['images'][0].update(max_extent=[1280,720,1]),
            lambda q:q['format_features'][0].update(optimal=0x8000),lambda q:q.update(completed_before_resource_creation=False),
            lambda q:q['images'][0].update(sample_counts=True)]
        for change in changes:
            forged=copy.deepcopy(q);change(forged)
            with self.assertRaises(ValueError):s.audit_queries(forged,'unorm')
    def test_types_paths_and_symlinks(self):
        with self.assertRaises(ValueError):s.equal(True,1,'bool integer')
        with self.assertRaises(ValueError):s.equal(44.0,44,'float integer')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'actual').write_bytes(b'x');(root/'link').symlink_to('actual')
            for name in ('../actual','/actual','link'):
                with self.assertRaises(ValueError):s.regular(root,name)
            self.assertEqual(s.regular(root,'actual',1),b'x')
    def test_raw_state_is_exact_typed_packet(self):
        for name in s.NAMES:
            st,pc,ubo=s.state(name)
            self.assertEqual(len(pc),28);self.assertEqual(len(ubo),1280)
            words=struct.unpack('<7I',pc);self.assertEqual(words[2],st['texture_index']);self.assertEqual(words[6],st['sampler_index'])
            self.assertEqual(struct.unpack_from('<2f',ubo),tuple(st['ubo_row0_xy']))
    def test_no_numeric_game_admission_in_completion(self):
        self.assertFalse(s.completion()['game_rendered']);self.assertEqual(s.completion()['readbacks'],72)


class ManifestAuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.patches=[patch.object(s,'WIDTH',16),patch.object(s,'HEIGHT',16),
                      patch.object(s.subprocess,'Popen',side_effect=AssertionError('No process allowed'))]
        for p in self.patches:p.start()
        self.modules=[dict(path='private-'+str(i),stage=stage,bytes=20,sha256=str(i)*64,fnv=str(i),entry='main',execution_model=0 if i==0 else 4) for i,stage in enumerate((1,16,16))]
        (self.root/'manifest.json').write_text(json.dumps(s.completion()))
        for mode in s.MODES:
            values={name:FinitePixelTests.images(self,name,mode) for name in s.NAMES}
            for suffix in ('original','diagnostic'):
                group=mode+'-'+suffix;folder=self.root/group;folder.mkdir();diag=suffix=='diagnostic'
                source=50 if mode=='srgb' else 44;view=44 if mode=='unorm' else 50;mut=mode=='mutable-srgb'
                q=MetadataTests.queries(self)
                for image in q['images']:
                    if image['format']!=37:image['extent']=[16,16,1]
                m=dict(schema_version=1,owned_fixture=True,game_rendered=False,diagnostic_uv=diag,
                    descriptor_buffer=False,state_snapshot_before_teardown=True,width=16,height=16,
                    source_image_format=source,attachment_view_format=view,pipeline_color_format=view,
                    destination_image_format=44,mutable_format=mut,source_create_flags=8 if mut else 0,
                    source_usage=19,destination_usage=3,view_formats=[44,50] if mut else [],
                    modules=[self.modules[0],self.modules[2 if diag else 1]],
                    pipeline=copy.deepcopy(s.PIPELINE),render_area=[0,0,16,16],dynamic_state=dict(viewport=True,scissor=True),
                    features=dict(**{key:True for key in s.base.FEATURES},descriptorBuffer=False,synchronization2=False),
                    descriptor_state=s.base.DESCRIPTORS,sampler_state=s.base.SAMPLERS,
                    texture_state=dict(width=8,height=8,format=37,mip_levels=1,files=['texture-A.rgba','texture-B.rgba','texture-B-rectangle.rgba']),
                    initialization=dict(method='raw_buffer_copy',source_bgra8=[7,5,3,255],bytes=1024,sha256=s.base.digest(s.SENTINEL*256)),
                    counts=dict(submissions=8,draws=6,image_uploads=3,target_initializations=6,source_readbacks=6,blits=6,destination_readbacks=6),
                    device=dict(vendor_id=0x10de,device_id=1,api_version=4206592,driver_version=1),queries=q,
                    validation=dict(debug_messenger=True,synchronization_validation=True,counters_include_teardown=True,errors=0,warnings=0,log_file='validation.log'),cases=[])
                (folder/'validation.log').write_text('')
                for fn,pixels in (('texture-A.rgba',s.texture_bytes(0)),('texture-B.rgba',s.texture_bytes(1)),('texture-B-rectangle.rgba',s.texture_bytes(1,True))):(folder/fn).write_bytes(pixels)
                for name in s.NAMES:
                    st,pc,ubo=s.state(name);c=dict(name=name,**st,bytes=1024,initialization='raw_buffer_copy',
                        source_copy_before_blit=True,serial_fence_completed=True,
                        blit=dict(source_layout=6,destination_layout=7,filter=0,region_count=1,source_subresource=[1,0,0,1],destination_subresource=[1,0,0,1],source_offsets=[[0,0,0],[16,16,1]],destination_offsets=[[0,0,0],[16,16,1]]))
                    for key,data in (('pc',pc),('ubo',ubo)):
                        fn=name+'.'+key+'.bin';(folder/fn).write_bytes(data);c.update({key+'_file':fn,key+'_sha256':s.base.digest(data)})
                    for key,data in (('source',values[name][1 if diag else 0]),('blit',values[name][3 if diag else 2])):
                        fn=name+'.'+key+'.bgra';(folder/fn).write_bytes(data);c.update({key+'_raw_file':fn,key+'_sha256':s.base.digest(data)})
                    m['cases'].append(c)
                (folder/'manifest.json').write_text(json.dumps(m))
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.tmp.cleanup()
    def test_all_seventy_two_synthetic_readbacks_independent_cpu_audit(self):
        result=s.audit_directory(self.root,self.modules)
        self.assertEqual(result['readbacks'],72);self.assertEqual(len(result['cases']),18)
        self.assertEqual(result['total_readback_bytes'],73728)
        self.assertFalse(result['game_numeric_uv_admitted']);self.assertFalse(result['effective_game_coverage_known'])
    def test_manifest_targeted_forgery_controls(self):
        folder=self.root/'mutable-srgb-diagnostic';path=folder/'manifest.json';original=path.read_text();m=json.loads(original)
        changes=[lambda m:m.update(source_image_format=50),lambda m:m.update(attachment_view_format=44),
            lambda m:m.update(pipeline_color_format=44),lambda m:m.update(view_formats=[50]),
            lambda m:m.update(mutable_format=False),lambda m:m['counts'].update(blits=0),
            lambda m:m['validation'].update(counters_include_teardown=False),lambda m:m['pipeline'].update(blend_enable=True),
            lambda m:m['pipeline'].update(color_write_mask=7),lambda m:m['cases'][0]['blit'].update(filter=1),
            lambda m:m['cases'][0].update(source_copy_before_blit=False),lambda m:m['cases'][0].update(serial_fence_completed=False),
            lambda m:m['cases'][0].update(pc_sha256='0'*64),lambda m:m['cases'][0].update(ubo_sha256='0'*64),
            lambda m:m['cases'][0].update(source_raw_file='../escape'),lambda m:m['cases'][0].update(texture_variant='rectangle')]
        for change in changes:
            forged=copy.deepcopy(m);change(forged);path.write_text(json.dumps(forged))
            with self.assertRaises(ValueError):s.audit_manifest(folder,'mutable-srgb-diagnostic',self.modules)
        path.write_text(original)
    def test_forged_zero_error_count_does_not_hide_validation_log(self):
        folder=self.root/'unorm-original';(folder/'validation.log').write_text('VUID-vkCmdBlitImage-srcImage-01999')
        with self.assertRaisesRegex(ValueError,'Validation error marker'):s.audit_manifest(folder,'unorm-original',self.modules)
    def test_modified_uploaded_texture_and_raw_packet_rejected(self):
        folder=self.root/'unorm-original'
        for file in ('texture-B-rectangle.rgba','A_clamp_zero.pc.bin','A_clamp_zero.ubo.bin'):
            path=folder/file;before=path.read_bytes();path.write_bytes(bytes([before[0]^1])+before[1:])
            with self.assertRaises(ValueError):s.audit_manifest(folder,'unorm-original',self.modules)
            path.write_bytes(before)
    def test_modified_raw_with_truthful_digest_still_fails_pixel_oracle(self):
        folder=self.root/'srgb-diagnostic';path=folder/'manifest.json';m=json.loads(path.read_text());c=m['cases'][0]
        raw=bytearray((folder/c['source_raw_file']).read_bytes());raw[2]=0
        (folder/c['source_raw_file']).write_bytes(raw);c['source_sha256']=s.base.digest(raw);path.write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError,'Finite SRGB pixel control failed'):s.audit_directory(self.root,self.modules)


class RunnerTests(unittest.TestCase):
    def args(self,root):
        for name in ('fixture','vertex','fragment','candidate','validator','runtime'):(root/name).write_bytes(name.encode())
        (root/'layer').mkdir()
        return argparse.Namespace(**{name:root/name for name in ('fixture','vertex','fragment','candidate','validator','runtime')},
            output=root/'out',validation_layer=root/'layer',**{n+'_sha256':s.base.digest((root/n).read_bytes()) for n in ('vertex','fragment','candidate')})
    def test_commands_two_environments_and_cleared_layer(self):
        with tempfile.TemporaryDirectory() as tmp:
            a=self.args(Path(tmp));modules=[dict(path=str(getattr(a,n))) for n in ('vertex','fragment','candidate')]
            alias_parent=Path(tmp)/'runtime-alias';alias_parent.mkdir()
            a.runtime=alias_parent/'..'/a.runtime.name
            resolved_runtime=str(a.runtime.resolve())
            with patch.object(s.base,'host_command',return_value=[]),patch.dict(s.os.environ,{'D4VR_TEST':'unsafe'}):
                native=s.launch_command(a,modules,'native');runtime=s.launch_command(a,modules,'steam-runtime')
            self.assertIn('D4VR_TEST',native);self.assertNotIn(resolved_runtime,native);self.assertIn(resolved_runtime,runtime)
            self.assertIn('VK_LAYER_VALIDATE_SYNC=1',native);self.assertEqual(runtime.count('--batch'),1)
    def test_mocked_runner_freezes_before_and_after_each_execution(self):
        import sampled_uv_probe
        with tempfile.TemporaryDirectory() as tmp:
            a=self.args(Path(tmp));calls=[]
            def execute(command,**kw):
                calls.append(command)
                text='' if len(calls)<=3 else json.dumps(s.completion())
                return argparse.Namespace(returncode=0,stdout=text,stderr='')
            def module(path,sha,stage):return dict(path=str(path),sha256=sha,stage=stage)
            audit=dict(cases=[dict(**{key+'_sha256':'a' for key in ('original_source','diagnostic_source','original_blit','diagnostic_blit')})])
            with patch.object(s.base,'verify_module',side_effect=module),patch.object(s.base,'host_command',return_value=[]),patch.object(sampled_uv_probe,'verify_candidate',return_value={'owned_mock':True}),patch.object(s.subprocess,'run',side_effect=execute),patch.object(s,'audit_directory',return_value=audit):
                proof=s.run(a)
            self.assertEqual(len(calls),5);self.assertEqual(proof['physical_readbacks'],144)
            self.assertEqual(proof['physical_source_sha256'],proof['final_source_sha256'])
            self.assertFalse(proof['game_numeric_uv_admitted'])
    def test_module_validation_failure_never_launches_fixture(self):
        import sampled_uv_probe
        with tempfile.TemporaryDirectory() as tmp:
            a=self.args(Path(tmp));calls=[]
            def refuse(command,**kw):calls.append(command);return argparse.Namespace(returncode=1,stdout='',stderr='bad module')
            with patch.object(s.base,'verify_module',return_value={}),patch.object(s.base,'host_command',return_value=[]),patch.object(sampled_uv_probe,'verify_candidate',return_value={}),patch.object(s.subprocess,'run',side_effect=refuse):
                with self.assertRaisesRegex(ValueError,'BEFORE GPU'):s.run(a)
            self.assertEqual(len(calls),1);self.assertFalse((a.output/'proof.json').exists())
    def test_changed_fixture_refused_without_physical_certificate(self):
        import sampled_uv_probe
        with tempfile.TemporaryDirectory() as tmp:
            a=self.args(Path(tmp))
            def mutate(command,**kw):a.fixture.write_bytes(b'changed');return argparse.Namespace(returncode=0,stdout='',stderr='')
            with patch.object(s.base,'verify_module',return_value={}),patch.object(s.base,'host_command',return_value=[]),patch.object(sampled_uv_probe,'verify_candidate',return_value={}),patch.object(s.subprocess,'run',side_effect=mutate):
                with self.assertRaisesRegex(ValueError,'frozen fixture'):s.run(a)
            self.assertFalse((a.output/'proof.json').exists())
    def test_existing_output_refused(self):
        import sampled_uv_probe
        with tempfile.TemporaryDirectory() as tmp:
            a=self.args(Path(tmp));a.output.mkdir()
            with patch.object(s.base,'verify_module',return_value={}),patch.object(sampled_uv_probe,'verify_candidate',return_value={}):
                with self.assertRaisesRegex(ValueError,'fresh output'):s.run(a)


if __name__=='__main__':unittest.main()
