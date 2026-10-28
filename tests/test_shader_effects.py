import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import frame_trace
import shader_effects as effects
from spirv_probe import string_words


def grammar_data():
    # A deliberately small test grammar; real audits use the external Khronos file.
    specifications = {
        11:('OpExtInstImport','Extension',False),12:('OpExtInst','Extension',True),
        15:('OpEntryPoint','Mode-Setting',False),21:('OpTypeInt','Type-Declaration',False),
        25:('OpTypeImage','Type-Declaration',False),32:('OpTypePointer','Type-Declaration',False),
        55:('OpFunctionParameter','Function',True),59:('OpVariable','Memory',True),
        61:('OpLoad','Memory',True),62:('OpStore','Memory',False),
        63:('OpCopyMemory','Memory',False),64:('OpCopyMemorySized','Memory',False),
        65:('OpAccessChain','Memory',True),71:('OpDecorate','Annotation',False),
        83:('OpCopyObject','Composite',True),99:('OpImageWrite','Image',False),
        169:('OpSelect','Relational_and_Logical',True),228:('OpAtomicStore','Atomic',False),
        234:('OpAtomicIAdd','Atomic',True),245:('OpPhi','Control-Flow',True),
        4160:('OpColorAttachmentReadEXT','Image',True),5398:('OpRawAccessChainNV','Memory',True)}
    return json.dumps({'magic_number':'0x07230203','instructions':[
        {'opcode':op,'opname':name,'class':kind,'operands':
         [{'kind':'IdResultType'},{'kind':'IdResult'}] if typed else []}
        for op,(name,kind,typed) in specifications.items()],
        'operand_kinds':[{'kind':'StorageClass','enumerants':[
            {'value':n,'enumerant':name} for n,name in
            [(0,'UniformConstant'),(2,'Uniform'),(3,'Output'),(6,'Private'),(7,'Function'),
             (12,'StorageBuffer'),(5349,'PhysicalStorageBuffer')]]}]}).encode()


def binary(extra=()):
    instructions=[(15,[0,90,*string_words('main')]),(21,[1,32,0]),
                  (32,[2,3,1]),(32,[3,7,1]),(32,[4,12,1]),(32,[5,5349,1]),
                  (59,[2,10,3]),(59,[3,11,7]),(59,[4,12,12]),(59,[5,13,5349])]
    words=[0x07230203,0x00010300,0,100,0]
    for op,args in [*instructions,*extra]: words += [((len(args)+1)<<16)|op,*args]
    return struct.pack('<'+str(len(words))+'I',*words)


class ShaderEffects(unittest.TestCase):
    def setUp(self): self.grammar=effects.Grammar(grammar_data())

    def test_pointer_storage_counts_include_copy_sources_and_unknown_writes(self):
        r=effects.analyze(binary([(62,[10,1]),(62,[11,1]),(62,[12,1]),(62,[13,1]),
                                  (62,[99,1]),(63,[11,12]),(64,[12,11,1]),(61,[1,20,12])]),self.grammar)
        self.assertEqual(r['explicit_external_pointer_write_instructions'],3)
        self.assertEqual(r['unresolved_pointer_write_instructions'],1)
        self.assertEqual(r['pointer_read_instructions_by_storage'],{'Function':1,'StorageBuffer':2})
        self.assertFalse(r['external_memory_writes_excluded']);self.assertFalse(r['replay_allowed'])

    def test_derived_and_parameter_pointer_types_are_used_without_claiming_bound_resources(self):
        r=effects.analyze(binary([(65,[4,20,12,1]),(83,[4,21,20]),(55,[4,22]),
                                  (169,[4,23,1,21,22]),(245,[4,24,23,90,22,91]),(62,[24,1])]),self.grammar)
        self.assertEqual(r['explicit_external_pointer_write_instructions'],1)
        self.assertEqual(r['pointer_write_instructions_by_storage'],{'StorageBuffer':1})
        self.assertFalse(r['bound_descriptor_images_resolved'])

    def test_variable_initializer_is_a_write(self):
        r=effects.analyze(binary([(59,[4,20,12,1])]),self.grammar)
        self.assertEqual(r['explicit_external_pointer_write_instructions'],1)

    def test_image_writes_and_atomics_are_positive_controls(self):
        r=effects.analyze(binary([(99,[50,51,52]),(228,[12,1,1,1]),(234,[1,20,12,1,1,1])]),self.grammar)
        self.assertEqual(r['image_write_instructions'],1)
        self.assertEqual(r['atomic_instructions'],{'OpAtomicIAdd':1,'OpAtomicStore':1})
        self.assertFalse(r['external_memory_writes_excluded'])

    def test_attachment_declarations_reads_and_direct_output_locations(self):
        r=effects.analyze(binary([(25,[20,1,6,0,0,0,2,0]),(4160,[1,21,1]),(71,[10,30,2])]),self.grammar)
        self.assertEqual(r['input_attachment_type_declarations'],1)
        self.assertEqual(r['attachment_read_instructions'],1)
        self.assertEqual(r['declared_direct_output_locations'],[2])
        self.assertFalse(r['output_interface_fully_resolved'])

    def test_unknown_opcodes_memory_extensions_and_extinst_remain_explicit(self):
        r=effects.analyze(binary([(65000,[]),(5398,[4,20,12,1,1,0]),
                                  (11,[30,*string_words('GLSL.std.450')]),(12,[1,31,30,13,1])]),self.grammar)
        self.assertEqual(r['unknown_opcodes'],{'65000':1})
        self.assertEqual(r['unmodeled_resource_instructions'],{'OpRawAccessChainNV':1})
        self.assertEqual(r['extended_instructions'],[{'set':'GLSL.std.450','instruction':13,'count':1}])
        self.assertFalse(r['extended_instruction_effects_audited'])

    def frame_inputs(self,directory,two=False):
        data=binary([(62,[10,1])]);names=['1111111111111111']+(['2222222222222222'] if two else [])
        for name in names:(directory/(name+'.spv')).write_bytes(data)
        catalog=directory/'catalog.json';catalog.write_text(json.dumps({'schema_version':1,'shader_identifier_algorithm':'a'*32,
            'shaders':[{'catalog_id':n,'code_bytes':len(data),'fnv1a64':frame_trace.fnv1a64(data),'module_identifier':'abcd'} for n in names]}))
        frame={'shader_identifier_algorithm':'a'*32,'pipelines':[{'id':1,'shaders':[
            {'stage':1,'known':False,'fnv1a64':'0'*16,'code_bytes':0,'module_identifier':'abcd'}]}],
            'request_sequence':1,'scope':'single_submission','present_before':0,'present_after':0,'truncated':False,
            'submissions':[{'complete':True,'result':0,'commands':[{'missing':False,'truncated':False,'operations':[
                {'kind':'dynamic_rendering','truncated':False,'area':[0,0,1,1],'colors':[],'depth':None,'stencil':None,'draws':[{'pipeline_id':1,'counts':[1,0,0,0,0,0,0,0]}]}]}]}]}
        return frame,catalog

    def test_frame_candidate_fingerprint_verified_and_still_not_byte_identity(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);frame,catalog=self.frame_inputs(directory)
            r=effects.audit_frame(frame,[0],directory,catalog,self.grammar)
            self.assertEqual(r['candidate_module_count'],1);self.assertEqual(r['stages_without_candidate'],0)
            self.assertFalse(r['module_identity_with_game_proven']);self.assertFalse(r['replay_allowed'])
            (directory/'1111111111111111.spv').write_bytes(binary())
            with self.assertRaisesRegex(ValueError,'differs'):effects.audit_frame(frame,[0],directory,catalog,self.grammar)

    def test_ambiguous_candidates_are_retained(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);frame,catalog=self.frame_inputs(directory,True)
            r=effects.audit_frame(frame,[0],directory,catalog,self.grammar)
            self.assertEqual(r['candidate_module_count'],2);self.assertEqual(r['stages_with_multiple_candidates'],1)
            self.assertFalse(r['external_memory_writes_excluded'])

    def test_missing_stages_libraries_and_scope_bounds(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);frame,catalog=self.frame_inputs(directory)
            frame['pipelines'][0]['missing_libraries']=True
            frame['pipelines'][0]['shaders'][0]['module_identifier']='ffff'
            r=effects.audit_frame(frame,[0],directory,catalog,self.grammar)
            self.assertEqual(r['stages_without_candidate'],2)
            for scopes in ([],[0,0],[2]):
                with self.assertRaises(ValueError):effects.audit_frame(frame,scopes,directory,catalog,self.grammar)

    def test_invalid_grammar_and_truncated_module_fail(self):
        bad=json.loads(grammar_data());bad['instructions'][0]['opcode']=32
        with self.assertRaises(ValueError):effects.Grammar(json.dumps(bad).encode())
        for data in (b'x',binary()+b'x',binary([(62,[10])]),binary([(32,[7])])):
            with self.assertRaises(ValueError):effects.analyze(data,self.grammar)
