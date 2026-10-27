"""Independent causal tests for the finite supplied SOURCE writer model.

The seed is authored here from the frozen precode contract, not from the
model's constructors or native fixture. Discovery is pure Python: no compiler,
process, GPU, existing capture or private work path is required.
"""
import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import source_emitted_writer_validate as model


U64_MAX = (1 << 64) - 1
FALSE_CLAIMS = {
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
}


def ref(kind, raw, generation):
    return {"kind": kind, "raw": raw, "generation": generation}


DEVICE = ref("device", 0xD080, 31)
VK_DEVICE = ref("vk_device", 0xE000, 37)
A = ref("resource", 0xA110, 41)
B = ref("resource", 0xA220, 43)
CPU = ref("heap", 0xC110, 47)
VISIBLE = ref("heap", 0xC220, 49)
ALLOCATOR = ref("allocator", 0xAC10, 53)
QUEUE = ref("queue", 0xCC10, 59)
COMMAND = ref("command", 0xCD10, 61)
ROOT = ref("root", 0xAA10, 67)
PSO = ref("pso", 0xAA20, 71)
LAYOUT = ref("pipeline_layout", 0xAA30, 73)
VS = ref("module", 0xAA40, 79)
FS = ref("module", 0xAA50, 83)
PIPELINE = ref("pipeline", 0xAA60, 89)
EPOCH = 701
COMMAND_BUFFER = 0xBABA
CPU_BASE = 0x100000
VISIBLE_BASE = 0x200000
GPU_BASE = 0x500000
STRIDE = 32
PSO_STATE = 0xF010


def span(values, pointer=0x800000):
    return {"count": len(values), "pointer": pointer,
            "values": copy.deepcopy(values), "complete": True}


def code(tag):
    return {"pointer": 0x900000 + tag, "bytes": 64,
            "sha256": hashlib.sha256(bytes([tag]) * 64).hexdigest(), "complete": True}


def bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def source_scope(command=None, epoch=None, site=None, owner=None, creator=None):
    return {"command": copy.deepcopy(command), "recording_epoch": epoch,
            "resource": None, "source_site": site,
            "pipeline_owner": copy.deepcopy(owner), "owner_creation_call": creator}


def source_after():
    return {"births": [], "recording_epoch": None, "command_buffer": None,
            "pipeline_state_pointer": None, "device_interface_pointer": None,
            "device_origin": None}


class OriginalBuilder:
    """Local independent E/R builder; call IDs deliberately differ from clock."""
    def __init__(self):
        self.calls = []
        self.clock = 0

    def begin(self, api, domain, args, *, parent=None, scope=None):
        self.clock += 1
        row = {"id": 101 + 7 * len(self.calls), "entry": self.clock, "return": None,
               "parent": parent, "api": api, "domain": domain,
               "source_before": copy.deepcopy(scope or source_scope()),
               "source_after": source_after(), "args": copy.deepcopy(args), "result": None}
        self.calls.append(row)
        return row

    def end(self, row, result=None, birth=None, parent=None, creator=None, **after):
        self.clock += 1
        row["return"] = self.clock
        row["result"] = copy.deepcopy(result)
        row["source_after"].update(copy.deepcopy(after))
        if birth is not None:
            row["source_after"]["births"] = [{"ref": copy.deepcopy(birth),
                "output_field": "output_raw", "parent": copy.deepcopy(parent),
                "owner_creator_call": creator}]
        return row

    def call(self, api, domain, args, result=None, **kwargs):
        row = self.begin(api, domain, args, parent=kwargs.pop("parent_call", None),
                         scope=kwargs.pop("scope", None))
        return self.end(row, result, **kwargs)


def snapshot(*, late=True, lazy=True, root_version=2, range_flags=3,
             width=1024, height=512, mips=11, selected_mip=2, ranges=False,
             setup_open=False):
    """Actual original-call graph for declared COM view only; no native view/query."""
    b = OriginalBuilder()
    device = b.begin("DeviceCreate", "source_original", {"output_pointer": 0x9100})
    b.call("VkDeviceCreate", "Vulkan_original",
           {"physical_device_raw": 0x7100, "output_pointer": 0x9200},
           {"vk_result": 0, "output_raw": VK_DEVICE["raw"]}, birth=VK_DEVICE,
           creator=device["id"], parent_call=device["id"],
           scope=source_scope(creator=device["id"]))
    b.end(device, {"hresult": 0, "output_raw": 0xD000}, birth=DEVICE,
          device_interface_pointer=DEVICE["raw"], device_origin="genuine_new")
    for api, obj in [("QueueCreate", QUEUE), ("AllocatorCreate", ALLOCATOR)]:
        b.call(api, "COM_original", {"device": DEVICE, "output_pointer": 0x9300},
               {"hresult": 0, "output_raw": obj["raw"]}, birth=obj, parent=DEVICE)
    for obj, count, flags in [(CPU, 2, 0), (VISIBLE, 1, 1)]:
        b.call("HeapCreate", "COM_original", {"device": DEVICE,
               "description": {"heap_type": 0, "count": count, "flags": flags, "node_mask": 0},
               "output_pointer": 0x9400}, {"hresult": 0, "output_raw": obj["raw"]},
               birth=obj, parent=DEVICE)
    description = {"dimension": 3, "width": width, "height": height,
                   "depth_or_array_size": 1, "mip_levels": mips, "format": 28,
                   "sample_count": 1, "flags": 0}
    for obj in (A, B):
        b.call("ResourceCreate", "COM_original",
               {"device": DEVICE, "description": description, "output_pointer": 0x9500},
               {"hresult": 0, "output_raw": obj["raw"]}, birth=obj, parent=DEVICE)
    root = b.begin("RootCreate", "COM_original",
                   {"device": DEVICE, "blob": code(1), "output_pointer": 0x9600})
    parameters = [
        {"parameter_type": 1, "visibility": 1, "ranges": None,
         "constants": {"shader_register": 5, "register_space": 2, "num32": 4}, "descriptor": None},
        {"parameter_type": 0, "visibility": 5,
         "ranges": span([{"range_type": 0, "num_descriptors": 1, "base_shader_register": 7,
                           "register_space": 2, "offset": 0, "flags": range_flags}]),
         "constants": None, "descriptor": None},
    ]
    root_desc = {"original_version": root_version, "flags": 1,
                 "parameters": span(parameters), "static_sampler_count": 0}
    b.call("RootParse", "source_original", {"blob": code(1), "description_output_pointer": 0x9700},
           {"status": 0, "description": root_desc}, parent_call=root["id"])
    b.call("PipelineLayoutCreate", "Vulkan_original",
           {"device": VK_DEVICE, "set_layout_raws": span([0x7200]), "output_pointer": 0x9800},
           {"vk_result": 0, "output_raw": LAYOUT["raw"]}, birth=LAYOUT,
           creator=root["id"], parent_call=root["id"], scope=source_scope(creator=root["id"]))
    b.end(root, {"hresult": 0, "output_raw": ROOT["raw"]}, birth=ROOT, parent=DEVICE)
    pso = b.begin("PSOCreate", "COM_original", {"device": DEVICE, "root": ROOT,
                  "vertex_bytecode": code(2), "pixel_bytecode": code(3), "output_pointer": 0x9900})

    def native_pipeline(parent, owner_pending=False):
        scope = source_scope(COMMAND if lazy else None, EPOCH if lazy else None,
                             owner=None if owner_pending else PSO,
                             creator=pso["id"] if owner_pending else None)
        for obj, tag in [(VS, 4), (FS, 5)]:
            b.call("ModuleCreate", "Vulkan_original",
                   {"device": VK_DEVICE, "code": code(tag), "output_pointer": 0x9A00},
                   {"vk_result": 0, "output_raw": obj["raw"]}, birth=obj,
                   parent=None if owner_pending else PSO,
                   creator=pso["id"] if owner_pending else None,
                   parent_call=parent["id"], scope=scope)
        b.call("PipelineCreate", "Vulkan_original", {"device": VK_DEVICE, "layout": LAYOUT,
               "stages": span([{"stage": 1, "module": VS, "entry": "main"},
                               {"stage": 16, "module": FS, "entry": "main"}]),
               "create_info_count": 1, "create_info_pointer": 0x9B00,
               "create_info_complete": True, "output_pointer": 0x9C00},
               {"vk_result": 0, "output_raw": PIPELINE["raw"]}, birth=PIPELINE,
               parent=None if owner_pending else PSO,
               creator=pso["id"] if owner_pending else None,
               parent_call=parent["id"], scope=scope)

    if not lazy:
        native_pipeline(pso, owner_pending=True)
    b.end(pso, {"hresult": 0, "output_raw": PSO["raw"]}, birth=PSO, parent=DEVICE,
          pipeline_state_pointer=PSO_STATE)
    b.call("CommandCreate", "COM_original", {"device": DEVICE,
           "allocator": ALLOCATOR if setup_open else None, "initial_pipeline": None,
           "output_pointer": 0x9D00}, {"hresult": 0, "output_raw": COMMAND["raw"]},
           birth=COMMAND, parent=DEVICE,
           scope=source_scope(site="CreateCommandList" if setup_open else "CreateCommandList1"))
    if setup_open:
        b.call("Close", "COM_original", {"command": COMMAND}, {"hresult": 0},
               scope=source_scope(COMMAND))
    b.call("Reset", "COM_original", {"command": COMMAND, "allocator": ALLOCATOR,
           "initial_pipeline": PSO}, {"hresult": 0}, scope=source_scope(COMMAND),
           recording_epoch=EPOCH, command_buffer=COMMAND_BUFFER)
    b.call("Increment", "COM_original", {"device": DEVICE, "heap_type": 0}, {"value": STRIDE})
    for api, obj, value, pointer in [("CPUStart", CPU, CPU_BASE, 0x9E00),
                                    ("CPUStart", VISIBLE, VISIBLE_BASE, 0x9E20),
                                    ("GPUStart", VISIBLE, GPU_BASE, 0x9E40)]:
        b.call(api, "source_original", {"heap": obj, "output_pointer": pointer},
               {"returned_pointer": pointer, "value": value})
    srv = {"format": 28, "view_dimension": 4, "component_mapping": 5768,
           "most_detailed_mip": selected_mip, "mip_levels": 1, "plane_slice": 0,
           "min_lod_clamp_bits": 0}
    for resource, destination in [(A, CPU_BASE), (B, CPU_BASE + STRIDE), (A, VISIBLE_BASE)]:
        b.call("CreateSRV", "COM_original", {"device": DEVICE, "resource": resource,
               "destination": destination, "description": srv})
    command_scope = source_scope(COMMAND, EPOCH)
    b.call("SetPipeline", "COM_original", {"command": COMMAND, "pipeline": PSO}, scope=command_scope)
    b.call("SetRoot", "COM_original", {"command": COMMAND, "root": ROOT}, scope=command_scope)
    b.call("SetHeaps", "COM_original", {"command": COMMAND, "heaps": span([VISIBLE])}, scope=command_scope)
    b.call("SetTable", "COM_original", {"command": COMMAND, "parameter": 1, "gpu_handle": GPU_BASE},
           scope=source_scope(COMMAND, EPOCH, site="default"))
    viewport = {"x_bits": bits(0), "y_bits": bits(0), "width_bits": bits(256),
                "height_bits": bits(144), "min_depth_bits": bits(0), "max_depth_bits": bits(1)}
    b.call("Viewports", "COM_original", {"command": COMMAND, "viewports": span([viewport])}, scope=command_scope)
    b.call("Scissors", "COM_original", {"command": COMMAND,
           "rects": span([{"left": 0, "top": 0, "right": 256, "bottom": 144}])}, scope=command_scope)
    b.call("RTV", "COM_original", {"command": COMMAND, "handles": span([0x7700]),
           "single_range": False}, scope=command_scope)
    draw = b.begin("Draw", "COM_original", {"command": COMMAND, "vertex_count": 3,
                   "instance_count": 1, "first_vertex": 0, "first_instance": 0}, scope=command_scope)
    lookup = b.begin("PipelineLookup", "source_original", {"state_pointer": PSO_STATE,
                     "dynamic_state_pointer": 0xF020, "dsv_format_pointer": 0,
                     "dynamic_flags_output_pointer": 0xF030}, parent=draw["id"],
                     scope=source_scope(COMMAND, EPOCH,
                         site="pipeline_get_or_create" if lazy else "pipeline_get", owner=PSO))
    if lazy:
        native_pipeline(lookup)
    b.end(lookup, {"value": PIPELINE["raw"], "dynamic_flags": 0})
    b.call("BindPipeline", "Vulkan_original", {"vk_command_buffer": COMMAND_BUFFER,
           "bind_point": 0, "pipeline": PIPELINE}, parent_call=draw["id"], scope=command_scope)
    emission = b.call("EmitDraw", "Vulkan_original", {"vk_command_buffer": COMMAND_BUFFER,
                      "vertex_count": 3, "instance_count": 1, "first_vertex": 0,
                      "first_instance": 0}, parent_call=draw["id"],
                      scope=source_scope(COMMAND, EPOCH, site="COM_DrawInstanced"))
    b.end(draw)
    b.call("Close", "COM_original", {"command": COMMAND}, {"hresult": 0}, scope=command_scope)
    if late:
        if ranges:
            b.call("CopyRanges", "COM_original", {"device": DEVICE, "heap_type": 0,
                   "sources": {"count": 1, "starts_pointer": 0xF100,
                               "starts": [CPU_BASE + STRIDE], "sizes_pointer": 0, "sizes": None},
                   "destinations": {"count": 1, "starts_pointer": 0xF200,
                                    "starts": [VISIBLE_BASE], "sizes_pointer": 0, "sizes": None}})
        else:
            b.call("CopySimple", "COM_original", {"device": DEVICE, "count": 1,
                   "destination": VISIBLE_BASE, "source": CPU_BASE + STRIDE, "heap_type": 0})
    execute = b.call("Execute", "COM_original", {"queue": QUEUE, "commands": span([COMMAND])})
    data = {"schema_version": 1, "purpose": "supplied_SOURCE_emitted_writer_originals",
            "capture": {"context": 191, "epoch": 193, "run_id": "19" * 16,
                        "enabled": True, "faulted": False, "gap": False, "overflow": False,
                        "calls_seen": len(b.calls), "original_returns": len(b.calls),
                        "calls_dropped": 0, "pending": 0, "clock": b.clock, "sealed_at": b.clock},
            "calls": b.calls,
            "ticket": {"emission_call": emission["id"], "execute_call": execute["id"]}}
    return data


def rows(data, api):
    return [row for row in data["calls"] if row["api"] == api]


def one(data, api):
    found = rows(data, api)
    if len(found) != 1:
        raise AssertionError((api, len(found)))
    return found[0]


def encode(data):
    return json.dumps(data, separators=(",", ":"), ensure_ascii=True, allow_nan=False).encode("utf-8")


def reframe(data):
    """Retime only local source edge positions after a causal graph mutation."""
    positions = sorted({value for row in data["calls"] for value in (row["entry"], row["return"])
                        if value is not None})
    mapping = {value: i + 1 for i, value in enumerate(positions)}
    for row in data["calls"]:
        row["entry"] = mapping[row["entry"]]
        if row["return"] is not None:
            row["return"] = mapping[row["return"]]
    data["calls"].sort(key=lambda row: row["entry"])
    original = [row for row in data["calls"] if row["domain"] != "source_observer"]
    data["capture"].update(calls_seen=len(original),
        original_returns=sum(row["return"] is not None for row in original),
        pending=sum(row["return"] is None for row in original), clock=len(positions), sealed_at=len(positions))
    return data


def insert_before(data, api, args, target="Draw", result=None, *, domain="COM_original", scope=None):
    anchor = rows(data, target)[-1]["entry"]
    row = {"id": max(call["id"] for call in data["calls"]) + 7,
           "entry": anchor - .2, "return": anchor - .1, "parent": None,
           "api": api, "domain": domain, "source_before": copy.deepcopy(scope or source_scope(COMMAND, EPOCH)),
           "source_after": source_after(), "args": copy.deepcopy(args), "result": copy.deepcopy(result)}
    data["calls"].append(row)
    reframe(data)
    return row


class SourceEmittedWriterTests(unittest.TestCase):
    def audit(self, data):
        self.last_wire = encode(data)
        result = model.analyze(self.last_wire)
        self.assertEqual(set(result["claims"]), FALSE_CLAIMS)
        self.assertTrue(all(value is False for value in result["claims"].values()))
        self.assertEqual(result["retained_input"], data)
        self.assertLessEqual(len(json.dumps(result, ensure_ascii=True).encode("utf-8")), 262144)
        return result

    def unknown(self, data):
        result = self.audit(data)
        self.assertIs(result["historical_declared_bound_view_known"], False)
        self.assertIsNone(result["declared_view"])
        return result

    def known(self, data, resource=B, extent=(256, 128)):
        result = self.audit(data)
        self.assertIs(result["historical_declared_bound_view_known"], True, result["diagnostics"])
        self.assertEqual(result["origin"], "COM_mutable_table")
        view = result["declared_view"]
        self.assertEqual(view["resource"], resource)
        self.assertEqual((view["declared_width"], view["declared_height"]), extent)
        self.assertEqual(view["root_parameter_index"], 1)
        self.assertEqual(view["selected_slot"], {"heap": VISIBLE, "index": 0, "cpu_handle": VISIBLE_BASE})
        self.assertEqual(view["cut"], one(data, "Execute")["entry"])
        return result

    def test_late_copy_selects_B_without_native_image_view_or_query(self):
        data = snapshot()
        self.assertFalse(any(row["api"] in {"ViewCreate", "GetSampledImageDescriptor"} for row in data["calls"]))
        result = self.known(data)
        self.assertEqual(result["declared_view"]["history_version"], one(data, "CopySimple")["id"])
        self.assertEqual(result["declared_view"]["original_view_call"], rows(data, "CreateSRV")[1]["id"])

    def test_direct_A_and_lazy_vs_ahead_pipeline_are_distinct_positives(self):
        self.known(snapshot(late=False), A)
        self.known(snapshot(lazy=False))

    def test_real_emission_missing_cannot_be_replaced_by_Draw_return(self):
        data = snapshot()
        data["ticket"]["emission_call"] = None
        self.unknown(data)
        data["ticket"]["emission_call"] = one(data, "Draw")["id"]
        self.unknown(data)

    def test_internal_and_deferred_origins_are_unknown_never_public_B(self):
        for site in ("CopyRenderPass_direct", "other"):
            with self.subTest(site=site):
                data = snapshot()
                one(data, "EmitDraw")["source_before"]["source_site"] = site
                self.unknown(data)

    def test_lazy_pipeline_owner_and_state_pointer_must_match_original_PSO(self):
        for field, value in [("pipeline_owner", ROOT), ("recording_epoch", EPOCH + 1)]:
            with self.subTest(field=field):
                data = snapshot()
                one(data, "PipelineLookup")["source_before"][field] = copy.deepcopy(value)
                self.unknown(data)
        data = snapshot()
        one(data, "PipelineLookup")["args"]["state_pointer"] += 8
        self.unknown(data)

    def test_lookup_cache_without_retained_birth_or_failed_output_is_unknown(self):
        for api in ("PipelineCreate", "ModuleCreate"):
            with self.subTest(api=api):
                data = snapshot()
                data["calls"].remove(rows(data, api)[-1])
                self.unknown(reframe(data))
        data = snapshot()
        one(data, "PipelineLookup")["result"] = {"value": 0, "dynamic_flags": None}
        self.unknown(data)

    def test_undefined_nonbirth_outputs_and_command_buffer_are_unknown(self):
        for api, key in [("RootParse", "description_output_pointer"),
                         ("PipelineLookup", "dynamic_flags_output_pointer")]:
            with self.subTest(api=api):
                data = snapshot()
                one(data, api)["args"][key] = 0
                self.unknown(data)
        data = snapshot()
        one(data, "EmitDraw")["args"]["vk_command_buffer"] += 1
        self.unknown(data)

    def test_actual_native_command_buffer_bind_cannot_be_hidden_by_SOURCE_scope(self):
        for scope in (source_scope(), source_scope(COMMAND, EPOCH + 1)):
            with self.subTest(scope=scope):
                data = snapshot()
                emission = one(data, "EmitDraw")
                override = copy.deepcopy(one(data, "BindPipeline"))
                override.update(id=max(row["id"] for row in data["calls"]) + 7,
                                entry=emission["entry"] - .2,
                                **{"return": emission["entry"] - .1})
                override["source_before"] = copy.deepcopy(scope)
                override["args"]["pipeline"] = ref("pipeline", 0xDEAD, 9001)
                data["calls"].append(override)
                self.unknown(reframe(data))

    def test_root_version1_original_null_and_version2_exact_volatile3(self):
        self.known(snapshot(root_version=1, range_flags=None))
        for version, flags in [(1, 3), (2, None), (2, 0), (2, 1), (2, 4), (2, 8), (3, 3)]:
            with self.subTest(version=version, flags=flags):
                self.unknown(snapshot(root_version=version, range_flags=flags))

    def test_same_root_preserves_but_null_or_ClearState_invalidates(self):
        data = snapshot()
        insert_before(data, "SetRoot", {"command": COMMAND, "root": ROOT})
        self.known(data)
        for api, args in [("SetRoot", {"command": COMMAND, "root": None}),
                          ("ClearState", {"command": COMMAND, "pipeline": PSO})]:
            with self.subTest(api=api):
                data = snapshot()
                insert_before(data, api, args)
                self.unknown(data)

    def test_overriding_root_table_and_MSB_setter_do_not_pass(self):
        for value in (GPU_BASE + STRIDE, GPU_BASE | (1 << 63)):
            with self.subTest(handle=value):
                data = snapshot()
                insert_before(data, "SetTable", {"command": COMMAND, "parameter": 1, "gpu_handle": value},
                              scope=source_scope(COMMAND, EPOCH, site="default"))
                self.unknown(data)

    def test_same_raw_original_COM_receiver_override_cannot_be_hidden_by_generation(self):
        for receiver in (dict(COMMAND, generation=None), dict(COMMAND, generation=9001),
                         dict(COMMAND, kind="root")):
            with self.subTest(receiver=receiver):
                data = snapshot()
                insert_before(data, "SetTable", {"command": receiver, "parameter": 1,
                              "gpu_handle": GPU_BASE + STRIDE},
                              scope=source_scope(COMMAND, EPOCH, site="default"))
                self.unknown(data)

    def test_wrong_root_parameter_static_index_and_incomplete_layout(self):
        data = snapshot()
        one(data, "SetTable")["args"]["parameter"] = 0
        self.unknown(data)
        for field, value in [("count", 3), ("pointer", 0), ("complete", False)]:
            with self.subTest(field=field):
                data = snapshot()
                params = one(data, "RootParse")["result"]["description"]["parameters"]
                params[field] = value
                if field in ("count", "pointer"):
                    params["complete"] = False
                self.unknown(data)

    def test_arrays_keep_original_count_pointer_completeness(self):
        for api, field in [("Execute", "commands"), ("SetHeaps", "heaps"),
                           ("Viewports", "viewports"), ("Scissors", "rects"), ("RTV", "handles")]:
            for mutate in ("incomplete", "null", "prefix"):
                with self.subTest(api=api, mutation=mutate):
                    data = snapshot()
                    target = one(data, api)["args"][field]
                    target["complete"] = False
                    if mutate == "null":
                        target["pointer"] = 0
                    elif mutate == "prefix":
                        target["count"] += 1
                    self.unknown(data)

    def test_copied_array_prefix_never_exceeds_actual_declared_count(self):
        for api, field, count in [("Viewports", "viewports", 0), ("Scissors", "rects", 0),
                ("SetHeaps", "heaps", 0), ("RTV", "handles", 0), ("Execute", "commands", 0),
                ("PipelineCreate", "stages", 1), ("PipelineLayoutCreate", "set_layout_raws", 0)]:
            with self.subTest(api=api), self.assertRaises(ValueError):
                data = snapshot()
                one(data, api)["args"][field].update(count=count, complete=False)
                self.audit(data)
        for which in ("parameters", "ranges"):
            with self.subTest(root=which), self.assertRaises(ValueError):
                data = snapshot()
                array = one(data, "RootParse")["result"]["description"]["parameters"]
                if which == "ranges":
                    array = array["values"][1]["ranges"]
                array["count"] -= 1
                self.audit(data)
        for which in ("sources", "destinations"):
            with self.subTest(copy=which), self.assertRaises(ValueError):
                data = snapshot(ranges=True)
                one(data, "CopyRanges")["args"][which]["count"] = 0
                self.audit(data)

    def test_range_NULL_sizes_and_explicit_zero_ranges_are_original(self):
        self.known(snapshot(ranges=True))
        data = snapshot(ranges=True)
        side = one(data, "CopyRanges")["args"]["sources"]
        side.update(count=2, starts=[CPU_BASE, CPU_BASE + STRIDE], sizes_pointer=0xF300, sizes=[0, 1])
        self.known(data)
        side["sizes"] = [1, 0]
        self.known(data, A)

    def test_copy_direction_type_and_array_totals_are_not_repaired(self):
        for field, value in [("heap_type", 1), ("count", 0), ("source", VISIBLE_BASE),
                             ("destination", CPU_BASE)]:
            with self.subTest(field=field):
                data = snapshot()
                one(data, "CopySimple")["args"][field] = value
                self.unknown(data)
        data = snapshot(ranges=True)
        one(data, "CopyRanges")["args"]["sources"]["sizes"] = [1]
        self.unknown(data)

    def test_source_history_after_returned_copy_cannot_retag_copied_version(self):
        data = snapshot()
        source = rows(data, "CreateSRV")[0]
        args = copy.deepcopy(source["args"])
        args["destination"] = CPU_BASE + STRIDE
        insert_before(data, "CreateSRV", args, target="Execute", scope=source_scope())
        self.known(data)

    def test_future_copy_cannot_rewrite_earlier_Execute_cut(self):
        data = snapshot()
        copy_call = one(data, "CopySimple")
        end = max(row["return"] for row in data["calls"])
        copy_call.update(entry=end + 1, **{"return": end + 2})
        self.known(reframe(data), A)

    def test_pending_or_overlapping_dependent_mutations_are_unknown(self):
        data = snapshot()
        one(data, "CopySimple")["return"] = None
        self.unknown(reframe(data))
        data = snapshot()
        write = rows(data, "CreateSRV")[1]
        write["return"] = one(data, "CopySimple")["return"] - .5
        self.unknown(reframe(data))

    def test_declared_extent_arithmetic_new_profile_exact_boundaries(self):
        for width, height, mips, mip, expected in [(1, 1, 1, 0, (1, 1)),
                (16384, 16384, 15, 14, (1, 1)), (9, 3, 4, 1, (4, 1)),
                (9, 3, 4, 3, (1, 1)), (1024, 512, 3, 2, (256, 128))]:
            with self.subTest(dimensions=(width, height), mip=mip):
                self.known(snapshot(width=width, height=height, mips=mips, selected_mip=mip), extent=expected)

    def test_invalid_or_broader_declared_views_stay_unknown(self):
        for kwargs in [{"width": 0}, {"width": 16385}, {"mips": 12}, {"mips": 0},
                       {"selected_mip": 11}, {"width": 1, "height": 1, "mips": 2, "selected_mip": 0}]:
            with self.subTest(kwargs=kwargs):
                self.unknown(snapshot(**kwargs))
        for key, value in [("format", 50), ("view_dimension", 5), ("mip_levels", 2), ("plane_slice", 1)]:
            with self.subTest(field=key):
                data = snapshot()
                for row in rows(data, "CreateSRV"):
                    row["args"]["description"][key] = value
                self.unknown(data)

    def test_zero_known_values_do_not_become_missing_IDs(self):
        data = snapshot()
        self.known(data)
        for api in ("Draw", "EmitDraw"):
            one(data, api)["args"]["vertex_count"] = 0
        self.known(data)
        for api in ("Increment", "GPUStart"):
            with self.subTest(api=api):
                data = snapshot()
                one(data, api)["result"]["value"] = 0
                self.unknown(data)

    def test_direct_emission_forwards_all_four_original_Draw_scalars(self):
        for field, value in [("vertex_count", 4), ("instance_count", 2),
                             ("first_vertex", 1), ("first_instance", 1)]:
            with self.subTest(field=field):
                data = snapshot()
                one(data, "EmitDraw")["args"][field] = value
                self.unknown(data)

    def test_pointer_outputs_unknown_null_mismatch_and_duplicates(self):
        for returned, value in [(0, None), (0xABCD, None), (0xABCD, CPU_BASE)]:
            with self.subTest(returned=returned, value=value):
                data = snapshot()
                getter = rows(data, "CPUStart")[0]
                getter["result"].update(returned_pointer=returned, value=value)
                if value is None:
                    self.unknown(data)
                else:
                    with self.assertRaises(ValueError):
                        self.audit(data)
        data = snapshot()
        getter = rows(data, "CPUStart")[0]
        insert_before(data, "CPUStart", getter["args"], target="CreateSRV", result=getter["result"],
                      domain="source_original", scope=source_scope())
        self.unknown(data)

    def test_descriptor_spans_checked_in_their_own_domains(self):
        data = snapshot()
        one(data, "GPUStart")["result"]["value"] = CPU_BASE
        one(data, "SetTable")["args"]["gpu_handle"] = CPU_BASE
        self.known(data)
        for mutate in ("CPU_alias", "CPU_overflow", "GPU_overflow"):
            with self.subTest(mutate=mutate):
                data = snapshot()
                if mutate == "CPU_alias":
                    rows(data, "CPUStart")[1]["result"]["value"] = CPU_BASE
                elif mutate == "CPU_overflow":
                    rows(data, "CPUStart")[0]["result"]["value"] = U64_MAX - 8
                else:
                    one(data, "GPUStart")["result"]["value"] = U64_MAX - 8
                self.unknown(data)

    def test_release_nonfinal_zero_and_command_epoch_mismatch(self):
        for obj in (DEVICE, B, VISIBLE, COMMAND, ALLOCATOR):
            for count in (0, 1):
                with self.subTest(kind=obj["kind"], count=count):
                    data = snapshot()
                    insert_before(data, "Release", {"object": obj}, target="Execute", result={"value": count},
                                  scope=source_scope())
                    self.unknown(data)
        data = snapshot()
        one(data, "EmitDraw")["source_before"]["recording_epoch"] += 1
        self.unknown(data)

    def test_setup_open_closed_and_failed_Reset_or_Close(self):
        self.known(snapshot(setup_open=True))
        for api in ("Reset", "Close"):
            data = snapshot()
            one(data, api)["result"]["hresult"] = -1
            if api == "Reset":
                one(data, api)["source_after"]["recording_epoch"] = None
                one(data, api)["source_after"]["command_buffer"] = None
            with self.subTest(api=api):
                self.unknown(data)

    def test_internal_device_output_is_not_COM_interface_and_creator_is_pending(self):
        data = snapshot()
        dev = one(data, "DeviceCreate")
        self.assertNotEqual(dev["result"]["output_raw"], DEVICE["raw"])
        child = one(data, "VkDeviceCreate")
        self.assertLess(child["return"], dev["return"])
        self.assertIsNone(child["source_after"]["births"][0]["parent"])
        self.known(data)
        for origin in ("singleton_existing", "unknown"):
            candidate = copy.deepcopy(data)
            one(candidate, "DeviceCreate")["source_after"]["device_origin"] = origin
            one(candidate, "DeviceCreate")["source_after"]["births"] = []
            self.unknown(candidate)

    def test_constructor_child_cannot_fake_an_already_live_parent(self):
        data = snapshot()
        child = one(data, "PipelineLayoutCreate")
        child["source_after"]["births"][0].update(parent=ROOT, owner_creator_call=None)
        child["source_before"]["owner_creation_call"] = None
        self.unknown(data)
        data = snapshot()
        one(data, "RootCreate")["result"] = {"hresult": -1, "output_raw": None}
        one(data, "RootCreate")["source_after"]["births"] = []
        self.unknown(data)

    def test_original_creation_device_receiver_cannot_be_overridden_by_owner_metadata(self):
        com = {"QueueCreate", "AllocatorCreate", "RootCreate", "PSOCreate",
               "HeapCreate", "ResourceCreate", "CommandCreate"}
        for api in sorted(com | {"ModuleCreate", "PipelineLayoutCreate", "PipelineCreate"}):
            for wrong in (ROOT, dict(DEVICE if api in com else VK_DEVICE,
                                    generation=9001)):
                with self.subTest(api=api, receiver=wrong):
                    data = snapshot()
                    rows(data, api)[0]["args"]["device"] = copy.deepcopy(wrong)
                    self.unknown(data)

    def test_creation_result_success_is_domain_exact_not_truthiness(self):
        for api, result_key in [("DeviceCreate", "hresult"), ("ResourceCreate", "hresult"),
                                ("PipelineCreate", "vk_result"), ("RootParse", "status")]:
            with self.subTest(api=api):
                data = snapshot()
                rows(data, api)[0]["result"][result_key] = 1
                self.unknown(data)
                rows(data, api)[0]["result"][result_key] = True
                with self.assertRaises(ValueError):
                    self.audit(data)

    def test_independent_reverse_returns_allowed_but_state_overlaps_unknown(self):
        data = snapshot()
        first, second = rows(data, "ResourceCreate")
        first["return"] = second["return"] + .1
        self.known(reframe(data))
        data = snapshot()
        one(data, "SetRoot")["return"] = one(data, "SetTable")["return"] + .1
        self.unknown(reframe(data))

    def test_same_raw_different_live_generation_never_selects_latest(self):
        data = snapshot()
        old = copy.deepcopy(B)
        new = dict(B, raw=A["raw"])

        def replace(value):
            if isinstance(value, dict):
                if value == old:
                    value.update(new)
                else:
                    for child in value.values():
                        replace(child)
            elif isinstance(value, list):
                for child in value:
                    replace(child)

        replace(data)
        rows(data, "ResourceCreate")[1]["result"]["output_raw"] = new["raw"]
        self.unknown(data)

    def test_known_empty_array_count_is_not_missing_pointer_or_one_element(self):
        for api, field in [("SetHeaps", "heaps"), ("Viewports", "viewports"),
                           ("Execute", "commands")]:
            for pointer in (0, 0x880000):
                with self.subTest(api=api, pointer=pointer):
                    data = snapshot()
                    one(data, api)["args"][field] = span([], pointer=pointer)
                    self.unknown(data)

    def test_contiguous_clock_endpoint_and_independent_CallID_namespace(self):
        data = snapshot()
        self.assertTrue(all(row["id"] != row["entry"] for row in data["calls"]))
        self.known(data)
        for mutate in ("endpoint", "offset", "hole", "counter"):
            with self.subTest(mutate=mutate):
                data = snapshot()
                if mutate == "endpoint":
                    data["capture"]["sealed_at"] += 1
                elif mutate == "offset":
                    for row in data["calls"]:
                        row["entry"] += 500
                        row["return"] += 500
                    data["capture"]["clock"] += 500
                    data["capture"]["sealed_at"] += 500
                elif mutate == "hole":
                    data["calls"].remove(one(data, "Increment"))
                    data["capture"]["calls_seen"] -= 1
                    data["capture"]["original_returns"] -= 1
                else:
                    data["capture"]["calls_seen"] += 1
                self.unknown(data)

    def test_pending_and_explicit_unavailable_capture_are_negative_diagnostics(self):
        for flag in ("faulted", "gap", "overflow"):
            data = snapshot()
            data["capture"][flag] = True
            self.unknown(data)
        data = snapshot()
        data["capture"]["enabled"] = False
        self.unknown(data)
        data = snapshot()
        one(data, "Execute")["return"] = None
        self.unknown(reframe(data))

    def test_strict_integer_and_void_return_domains(self):
        for invalid in (True, 1.0, -1, 1 << 64):
            with self.subTest(invalid=invalid):
                data = snapshot()
                data["capture"]["context"] = invalid
                with self.assertRaises(ValueError):
                    self.audit(data)
        for invalid in (0, False, {"hresult": 0}):
            data = snapshot()
            one(data, "Execute")["result"] = invalid
            with self.subTest(void_result=invalid), self.assertRaises(ValueError):
                self.audit(data)

    def test_malformed_raw_JSON_and_closed_shapes(self):
        raw = encode(snapshot())
        duplicate_valid_root = b'{"schema_version":1,' + raw[1:]
        for invalid in (raw.decode().encode("utf-16"), duplicate_valid_root,
                        b'{"x":NaN}', b'{"x":Infinity}', b'\xff', raw[:-1], snapshot()):
            with self.subTest(kind=type(invalid).__name__), self.assertRaises(ValueError):
                model.analyze(invalid)
        for container in ([], {}, True):
            data = snapshot()
            one(data, "Execute")["api"] = container
            with self.subTest(api=container), self.assertRaises(ValueError):
                self.audit(data)
        data = snapshot()
        one(data, "EmitDraw")["args"]["source_site"] = "COM_DrawInstanced"
        with self.assertRaises(ValueError):
            self.audit(data)

    def test_bounded_input_and_original_array_tail_are_not_truncated(self):
        with self.assertRaises(ValueError):
            model.analyze(b" " * 65537)
        data = snapshot()
        one(data, "Execute")["args"]["commands"]["values"] *= 5
        one(data, "Execute")["args"]["commands"]["count"] = 5
        with self.assertRaises(ValueError):
            self.audit(data)
        data = snapshot()
        target = one(data, "Execute")["args"]["commands"]
        target.update(count=5, complete=False)
        self.unknown(data)
        for raw in (b"[" * 1100 + b"]" * 1100,
                    b"[" + b",".join(b"0" for _ in range(4097)) + b"]"):
            with self.subTest(boundary="nested_or_nodes"), self.assertRaises(ValueError):
                model.analyze(raw)

    def test_UTF8_text_and_whitespace_values_not_reencoded_child_proof(self):
        data = snapshot()
        report = self.known(data)
        self.assertEqual(report, model.analyze("\n  " + encode(data).decode("utf-8") + "\t\n"))
        with self.assertRaises(ValueError):
            model.analyze("\ud800")

    def test_exact_record_budget_and_observer_marks_without_false_originals(self):
        data = snapshot()
        data["calls"] = []
        data["ticket"] = {"emission_call": None, "execute_call": None}
        while len(data["calls"]) < 64:
            endpoint = len(data["calls"]) + 1
            data["calls"].append({"id": 101 + 7 * len(data["calls"]),
                "entry": endpoint, "return": endpoint, "parent": None,
                "api": "Unsupported", "domain": "source_observer",
                "source_before": source_scope(), "source_after": source_after(),
                "args": {"original_name": "unselected_original", "affects_selected_scope": False},
                "result": None})
        reframe(data)
        self.assertLess(len(encode(data)), 65536)
        self.unknown(data)
        extra = copy.deepcopy(data["calls"][-1])
        extra["id"] += 7
        extra["entry"] += 1
        extra["return"] += 1
        data["calls"].append(extra)
        reframe(data)
        with self.assertRaises(ValueError):
            self.audit(data)

    def test_report_detached_after_eighty_reanalyses_no_global_registry_invented(self):
        data = snapshot()
        original = copy.deepcopy(data)
        held = self.known(data)
        held_raw = json.dumps(held, sort_keys=True)
        for i in range(80):
            churn = snapshot(late=bool(i % 2))
            churn["capture"]["context"] += i + 1
            churn["capture"]["epoch"] += i + 1
            churn["capture"]["run_id"] = format(i + 1, "032x")
            self.known(churn, B if i % 2 else A)
        data["calls"][0]["args"]["output_pointer"] = 0
        self.assertEqual(held_raw, json.dumps(held, sort_keys=True))
        held["retained_input"]["capture"]["enabled"] = False
        self.assertEqual(original, model.analyze(encode(original))["retained_input"])

    def test_silent_overwrite_twins_are_deliberately_indistinguishable(self):
        first = encode(snapshot())
        second = bytes(bytearray(first))
        self.assertEqual(first, second)
        before = model.analyze(first)
        after_unobserved_overwrite = model.analyze(second)
        self.assertEqual(before, after_unobserved_overwrite)
        self.assertTrue(before["historical_declared_bound_view_known"])
        self.assertTrue(all(value is False for value in before["claims"].values()))


if __name__ == "__main__":
    unittest.main()
