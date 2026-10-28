"""Independent supplied histories; these are not source captures or vendor ABI tests."""
import copy
import hashlib
import importlib
import json
import unittest
from unittest import mock


CLAIMS = {
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
}


def encoded(value):
    return json.dumps(value, separators=(",", ":"), ensure_ascii=True, allow_nan=False).encode()


def reference(kind, raw, generation):
    return {"kind": kind, "raw": raw, "generation": generation}


def scope(command=None, epoch=None, ordinal=None, **extra):
    out = {"command": copy.deepcopy(command), "recording_epoch_ref": epoch,
           "resource": None, "source_site": None, "pipeline_owner": None,
           "owner_creation_call": None, "reset_ordinal": ordinal, "pipeline_route": None}
    out.update(copy.deepcopy(extra))
    return out


def aftermath():
    return {"births": [], "recording_epoch_ref": None, "command_buffer": None,
            "pso_pointer": None, "device_interface_pointer": None, "device_origin": None,
            "reset_ordinal": None, "root_signature_pointer": None, "pso_root_pointer": None}


def span(values, pointer):
    return {"count": len(values), "pointer": pointer, "values": copy.deepcopy(values), "complete": True}


def code(label, pointer):
    payload = (label.encode() * 64)[:64]
    return {"pointer": pointer, "bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(), "complete": True}


class History:
    """A fixture writer, independent of the model and precode documentary examples."""
    def __init__(self):
        self.calls = []
        self.clock = 0
        self.stack = []

    def begin(self, api, args, result, source=None, domain="COM_original"):
        self.clock += 1
        row = {"id": 101 + 17 * len(self.calls), "entry": self.clock, "return": None,
               "parent": self.stack[-1]["id"] if self.stack else None, "api": api,
               "domain": domain, "source_before": copy.deepcopy(source or scope()),
               "source_after": aftermath(), "args": copy.deepcopy(args), "result": copy.deepcopy(result)}
        self.calls.append(row)
        self.stack.append(row)
        return row

    def end(self, row):
        assert self.stack.pop() is row
        self.clock += 1
        row["return"] = self.clock
        return row

    def call(self, api, args, result=None, source=None, domain="COM_original"):
        return self.end(self.begin(api, args, result, source, domain))


def supplied_history(late=False):
    h = History()
    canonical = reference("device", 0x1100, 7)
    device = reference("device", 0x2180, 7)  # True original COM receiver, NOT canonical raw.
    vk = reference("vk_device", 0x3300, 9)
    queue = reference("queue", 0x4400, 13)
    allocator = reference("allocator", 0x5500, 15)
    command = reference("command", 0x6600, 17)
    root = reference("root", 0x7700, 19)
    pso = reference("pso", 0x8800, 23)
    layout = reference("pipeline_layout", 0x9900, 29)
    pipeline = reference("pipeline", 0xaa00, 31)
    modules = [reference("module", 0xbb00, 37), reference("module", 0xcc00, 41)]

    def birth(row, ref, owner=None, creator=None):
        row["source_after"]["births"] = [{"ref": copy.deepcopy(ref), "output_field": "output_raw",
                                            "parent": copy.deepcopy(owner), "owner_creator_call": creator}]

    dc = h.begin("DeviceCreate", {"output_pointer": 0x100000},
                 {"hresult": 0, "output_raw": canonical["raw"]}, domain="source_original")
    vc = h.call("VkDeviceCreate", {"physical_device_raw": 0x1200, "output_pointer": 0x100008},
                {"vk_result": 0, "output_raw": vk["raw"]}, domain="Vulkan_original")
    birth(vc, vk, creator=dc["id"])
    h.end(dc)
    birth(dc, canonical)
    dc["source_after"].update(device_interface_pointer=device["raw"], device_origin="genuine_new")
    for api, ref in [("QueueCreate", queue), ("AllocatorCreate", allocator)]:
        r = h.call(api, {"device": device, "output_pointer": 0x100100 + ref["raw"], "type": 0},
                   {"hresult": 0, "output_raw": ref["raw"]})
        birth(r, ref, canonical)
    cc = h.begin("CommandCreate", {"device": device, "allocator": allocator, "initial_pipeline": None,
                 "output_pointer": 0x100200, "node_mask": 0, "type": 0,
                 "requested_iid": "5b160d0f-ac1b-4185-8ba8-b3ae42a5a455"},
                 {"hresult": 0, "output_raw": command["raw"]}, scope(source_site="CreateCommandList"))
    unborn = dict(command, generation=None)
    cr = h.call("Reset", {"command": unborn, "allocator": allocator, "initial_pipeline": None},
                {"hresult": 0}, scope(unborn, source_site="command_constructor_reset", owner_creation_call=cc["id"]))
    cr["source_after"].update(reset_ordinal=1, command_buffer=0xd100)
    h.end(cc)
    birth(cc, command, canonical)

    resources = {}
    for i, name in enumerate(("A", "uploadA", "B", "uploadB", "target", "readback")):
        image = name in ("A", "B", "target")
        target = name == "target"
        ref = reference("resource", 0x20000 + 0x100 * i, 43 + 3 * i)
        resources[name] = ref
        desc = {"dimension": 3 if image else 1, "alignment": 0,
                "width": 256 if target else (8 if image else (147456 if name == "readback" else 1824)),
                "height": 144 if target else (8 if image else 1), "depth_or_array_size": 1,
                "mip_levels": 1, "format": 87 if target else (28 if image else 0), "sample_count": 1,
                "sample_quality": 0, "layout": 0 if image else 1, "flags": 1 if target else 0}
        ht = 1 if image else (3 if name == "readback" else 2)
        r = h.call("ResourceCreate", {"device": device, "heap_properties_pointer": 0x110000 + 64 * i,
                   "heap_properties": {"type": ht, "cpu_page_property": 0, "memory_pool_preference": 0,
                                       "creation_node_mask": 0, "visible_node_mask": 0},
                   "heap_flags": 0, "description_pointer": 0x111000 + 64 * i, "description": desc,
                   "initial_state": 0x400 if (name in ("A", "B", "readback")) else (0x800 if target else 0xac3),
                   "clear_value_pointer": 0, "requested_iid": "696442be-a72e-4059-bc79-5b5c98040fad",
                   "output_pointer": 0x112000 + 8 * i}, {"hresult": 0, "output_raw": ref["raw"]})
        birth(r, ref, canonical)

    heaps = {}
    for i, (name, typ, count, flags) in enumerate((("cpu", 0, 2, 0), ("visible", 0, 1, 1),
                                                  ("sampler", 1, 2, 1), ("rtv", 2, 1, 0))):
        ref = reference("heap", 0x30000 + 0x100 * i, 67 + 3 * i)
        heaps[name] = ref
        r = h.call("HeapCreate", {"device": device, "description": {"heap_type": typ, "count": count,
                   "flags": flags, "node_mask": 0}, "output_pointer": 0x113000 + 8 * i,
                   "description_pointer": 0x114000 + 32 * i,
                   "requested_iid": "8efb471d-616c-4f49-90f7-127bb763fa51"},
                   {"hresult": 0, "output_raw": ref["raw"]})
        birth(r, ref, canonical)
    cpu = {"cpu": 0x40000, "visible": 0x41000, "sampler": 0x42000, "rtv": 0x43000}
    gpu = {"visible": 0x50000, "sampler": 0x51000}
    h.call("Increment", {"device": device, "heap_type": 0}, {"value": 32})
    def getter(api, name, value):
        pointer = 0x115000 + 16 * len(h.calls)
        return h.call(api, {"heap": heaps[name], "output_pointer": pointer},
                      {"returned_pointer": pointer, "value": value}, domain="source_original")
    getter("CPUStart", "cpu", cpu["cpu"])
    getter("CPUStart", "visible", cpu["visible"])
    getter("GPUStart", "visible", gpu["visible"])
    h.call("Increment", {"device": device, "heap_type": 1}, {"value": 16})
    getter("CPUStart", "sampler", cpu["sampler"])
    getter("GPUStart", "sampler", gpu["sampler"])
    getter("CPUStart", "rtv", cpu["rtv"])
    for i, address in enumerate((3, 1)):
        h.call("CreateSampler", {"device": device, "description_pointer": 0x116000 + 64 * i,
               "description": {"filter": 0, "address_u": address, "address_v": address, "address_w": address,
                               "mip_lod_bias_bits": 0, "max_anisotropy": 1, "comparison_func": 8,
                               "border_color_bits": [0, 0, 0, 0], "min_lod_bits": 0, "max_lod_bits": 0},
               "description_complete": True, "destination": cpu["sampler"] + 16 * i})
    h.call("CreateRTV", {"device": device, "resource": resources["target"],
                        "description_pointer": 0, "destination": cpu["rtv"]})

    blob = code("root", 0x120000)
    rc = h.begin("RootCreate", {"device": device, "blob": blob, "output_pointer": 0x120100, "node_mask": 0},
                 {"hresult": 0, "output_raw": root["raw"]})
    params = []
    for i, typ in enumerate((0, 3)):
        params.append({"parameter_type": 0, "visibility": 5,
                       "ranges": span([{"range_type": typ, "num_descriptors": 1, "base_shader_register": 0,
                                        "register_space": 0, "offset": 0, "flags": None}], 0x120200 + 32 * i),
                       "constants": None, "descriptor": None})
    params.append({"parameter_type": 1, "visibility": 5, "ranges": None,
                   "constants": {"shader_register": 0, "register_space": 0, "num32": 2}, "descriptor": None})
    h.call("RootParse", {"blob": blob, "description_output_pointer": 0x120300},
           {"status": 0, "description": {"original_version": 1, "flags": 1,
                                        "parameters": span(params, 0x120400), "static_sampler_count": 0}},
           domain="source_original")
    layout_row = h.call("PipelineLayoutCreate", {"device": vk, "create_info_pointer": 0x121000,
                        "s_type": 30, "p_next": 0, "flags": 0, "set_layout_raws": span([0x141000, 0x142000], 0x121100),
                        "push_constant_ranges": span([{"stage_flags": 16, "offset": 0, "size": 16}], 0x121200),
                        "allocator_pointer": 0, "output_pointer": 0x121300},
                        {"vk_result": 0, "output_raw": layout["raw"]},
                        scope(source_site="root_graphics_layout", owner_creation_call=rc["id"]), "Vulkan_original")
    birth(layout_row, layout, creator=rc["id"])
    h.end(rc)
    birth(rc, root, canonical)
    rc["source_after"]["root_signature_pointer"] = 0x17000

    pc = h.begin("PSOCreate", {"device": device, "root": root, "vertex_bytecode": code("VS-DXIL", 0x130000),
                 "pixel_bytecode": code("FS-DXIL", 0x131000), "output_pointer": 0x132000,
                 "rtv_formats": span([87], 0x132100), "sample_count": 1, "sample_quality": 0},
                 {"hresult": 0, "output_raw": pso["raw"]})
    for i, ref in enumerate(modules):
        r = h.call("ModuleCreate", {"device": vk, "create_info_pointer": 0x133000 + i * 128, "s_type": 16,
                   "p_next": 0, "flags": 0, "code": code("VS-SPIRV" if i == 0 else "FS-SPIRV", 0x134000 + i * 4096),
                   "allocator_pointer": 0, "output_pointer": 0x136000 + i * 8},
                   {"vk_result": 0, "output_raw": ref["raw"]},
                   scope(owner_creation_call=pc["id"]), "Vulkan_original")
        birth(r, ref, creator=pc["id"])
    rendering = {"kind": "rendering", "pointer": 0x137000, "s_type": 1000044002, "next_pointer": 0,
                 "payload": {"view_mask": 0, "color_formats": span([44], 0x137100), "depth_format": 0, "stencil_format": 0}}
    stages = [{"s_type": 18, "p_next": 0, "flags": 0, "stage": stage, "module": ref,
               "name_pointer": 0x138000 + i * 16, "entry": "main", "specialization_pointer": 0}
              for i, (stage, ref) in enumerate(zip((1, 16), modules))]
    native = h.call("PipelineCreate", {"device": vk, "cache_raw": 0, "create_info_count": 1,
                    "create_info_pointer": 0x139000, "create_info_complete": True, "s_type": 28, "flags": 0,
                    "p_next": {"pointer": rendering["pointer"], "nodes": [rendering], "complete": True},
                    "library_info": None, "layout": layout, "render_pass_raw": 0, "subpass": 0,
                    "base_pipeline_raw": 0, "base_pipeline_index": -1, "stages": span(stages, 0x13a000),
                    "allocator_pointer": 0, "output_pointer": 0x13b000,
                    "state_pointers": {"vertex_input": 0x13c000, "input_assembly": 0x13c100, "tessellation": 0,
                                       "viewport": 0x13c200, "rasterization": 0x13c300, "multisample": 0x13c400,
                                       "depth_stencil": 0x13c500, "color_blend": 0x13c600, "dynamic": 0x13c700}},
                    {"vk_result": 0, "output_raw": pipeline["raw"]},
                    scope(owner_creation_call=pc["id"], pipeline_route="legacy_monolithic"), "Vulkan_original")
    birth(native, pipeline, creator=pc["id"])
    for ref in modules:
        h.call("ModuleDestroy", {"device": vk, "object": ref, "allocator_pointer": 0},
               source=scope(owner_creation_call=pc["id"]), domain="Vulkan_original")
    h.end(pc)
    birth(pc, pso, canonical)
    pc["source_after"].update(pso_pointer=0x18000, pso_root_pointer=0x17000)

    h.call("Close", {"command": command}, {"hresult": 0}, scope(command, ordinal=1))
    h.call("Execute", {"queue": queue, "commands": span([command], 0x140000)})
    h.call("AllocatorReset", {"allocator": allocator}, {"hresult": 0})
    reset = h.call("Reset", {"command": command, "allocator": allocator, "initial_pipeline": None},
                   {"hresult": 0}, scope(command, ordinal=1))
    reset["source_after"].update(recording_epoch_ref=reset["id"], reset_ordinal=2, command_buffer=0xd200)
    def cs(**kw):
        return scope(command, reset["id"], 2, **kw)
    h.call("SetPipeline", {"command": command, "pipeline": pso}, source=cs())
    srv = {"format": 28, "view_dimension": 4, "component_mapping": 5768, "most_detailed_mip": 0,
           "mip_levels": 1, "plane_slice": 0, "min_lod_clamp_bits": 0}
    for i, (name, dest) in enumerate((("A", cpu["cpu"]), ("B", cpu["cpu"] + 32), ("A", cpu["visible"]))):
        h.call("CreateSRV", {"device": device, "resource": resources[name], "destination": dest,
                            "description": srv, "description_pointer": 0x141000 + i * 64})
    h.call("SetRoot", {"command": command, "root": root}, source=cs())
    h.call("SetHeaps", {"command": command, "heaps": span([heaps["visible"], heaps["sampler"]], 0x142000)}, source=cs())
    for i, name in enumerate(("visible", "sampler")):
        h.call("SetTable", {"command": command, "parameter": i, "gpu_handle": gpu[name]},
               source=cs(source_site="default"))
    h.call("SetConstants", {"command": command, "parameter": 2,
                           "words": span([0x3e000000, 0xbe000000], 0x143000), "destination_offset": 0}, source=cs())
    h.call("Topology", {"command": command, "topology": 4}, source=cs())
    vp = {"x_bits": 0, "y_bits": 0, "width_bits": 0x43800000, "height_bits": 0x43100000,
          "min_depth_bits": 0, "max_depth_bits": 0x3f800000}
    h.call("Viewports", {"command": command, "viewports": span([vp], 0x144000)}, source=cs())
    h.call("Scissors", {"command": command, "rects": span([{"left": 0, "top": 0, "right": 256, "bottom": 144}], 0x145000)}, source=cs())
    h.call("RTV", {"command": command, "handles": span([cpu["rtv"]], 0x146000), "single_range": 1, "dsv_pointer": 0}, source=cs())
    draw_args = {"vertex_count": 3, "instance_count": 1, "first_vertex": 0, "first_instance": 0}
    draw = h.begin("Draw", dict(draw_args, command=command), None, cs(source_site="COM_DrawInstanced"))
    h.call("PipelineLookup", {"state_pointer": 0x18000, "dynamic_state_pointer": 0x147000,
           "dsv_format_pointer": 0, "dynamic_flags_output_pointer": 0x147100},
           {"value": pipeline["raw"], "dynamic_flags": 3}, cs(source_site="pipeline_get", pipeline_owner=pso), "source_original")
    h.call("BindPipeline", {"vk_command_buffer": 0xd200, "bind_point": 0, "pipeline": pipeline},
           source=cs(pipeline_owner=pso), domain="Vulkan_original")
    native_vp = dict(vp, y_bits=0x43100000, height_bits=0xc3100000)
    h.call("VkViewportCount", {"vk_command_buffer": 0xd200, "viewports": span([native_vp], 0x148000)},
           source=cs(pipeline_owner=pso), domain="Vulkan_original")
    h.call("VkScissorCount", {"vk_command_buffer": 0xd200,
           "rects": span([{"offset_x": 0, "offset_y": 0, "width": 256, "height": 144}], 0x149000)},
           source=cs(pipeline_owner=pso), domain="Vulkan_original")
    emit = h.call("EmitDraw", dict(draw_args, vk_command_buffer=0xd200),
                  source=cs(source_site="COM_DrawInstanced", pipeline_owner=pso), domain="Vulkan_original")
    h.end(draw)
    h.call("Close", {"command": command}, {"hresult": 0}, cs())
    if late:
        h.call("CopySimple", {"device": device, "count": 1, "destination": cpu["visible"],
                              "source": cpu["cpu"] + 32, "heap_type": 0})
    execute = h.call("Execute", {"queue": queue, "commands": span([command], 0x14a000)})
    return {"schema_version": 2, "purpose": "supplied_SOURCE_owned_first_writer_originals",
            "capture": {"context": 211, "context_epoch": 223, "run_id": "c" * 32, "enabled": True,
                        "faulted": False, "gap": False, "overflow": False, "calls_seen": len(h.calls),
                        "original_returns": len(h.calls), "calls_dropped": 0, "pending": 0,
                        "clock": h.clock, "sealed_at": h.clock}, "calls": h.calls,
            "ticket": {"reset_call": reset["id"], "emission_call": emit["id"], "execute_call": execute["id"]}}


def rows(data, api):
    return [r for r in data["calls"] if r["api"] == api]


def reclock(data):
    """Preserve chosen parent intervals, never derive call IDs from the clock."""
    children = {}
    roots = []
    for row in data["calls"]:
        (roots if row["parent"] is None else children.setdefault(row["parent"], [])).append(row)
    n = 0
    def visit(row):
        nonlocal n
        n += 1
        row["entry"] = n
        for child in children.get(row["id"], []):
            visit(child)
        n += 1
        row["return"] = n
    for row in roots:
        visit(row)
    data["calls"].sort(key=lambda r: r["entry"])
    originals = [r for r in data["calls"] if r["api"] != "ScopeGap"]
    data["capture"].update(calls_seen=len(originals), original_returns=len(originals), pending=0, clock=n, sealed_at=n)
    return data


def new_row(data, api, args, result=None, source=None, domain="COM_original"):
    return {"id": max(r["id"] for r in data["calls"]) + 101, "entry": 0, "return": 0,
            "parent": None, "api": api, "domain": domain, "source_before": copy.deepcopy(source or scope()),
            "source_after": aftermath(), "args": copy.deepcopy(args), "result": copy.deepcopy(result)}


def insert_before(data, target, row):
    data["calls"].insert(data["calls"].index(target), row)
    return reclock(data)


def concurrent_before_return(data, enclosing, row):
    """An independently retained original overlaps an existing call; no fake parent."""
    events = sorted((tick, c["id"], edge) for c in data["calls"]
                    for tick, edge in ((c["entry"], "entry"), (c["return"], "return")))
    pos = next(i for i, (_, cid, edge) in enumerate(events) if cid == enclosing["id"] and edge == "return")
    events[pos:pos] = [(0, row["id"], "entry"), (0, row["id"], "return")]
    data["calls"].append(row)
    by_id = {c["id"]: c for c in data["calls"]}
    for tick, (_, cid, edge) in enumerate(events, 1):
        by_id[cid][edge] = tick
    data["calls"].sort(key=lambda c: c["entry"])
    data["capture"].update(calls_seen=len(data["calls"]), original_returns=len(data["calls"]),
                           clock=len(events), sealed_at=len(events), pending=0)
    return data


def expected_cold_report(data):
    """Contract output assembled independently, with no model-derived oracle fields."""
    srv = rows(data, "CreateSRV")[-1]
    heaps = [r["source_after"]["births"][0]["ref"] for r in rows(data, "HeapCreate")]
    target = rows(data, "ResourceCreate")[4]["source_after"]["births"][0]["ref"]
    const = rows(data, "SetConstants")[0]
    table = rows(data, "SetTable")[1]
    viewport = rows(data, "VkViewportCount")[0]
    scissor = rows(data, "VkScissorCount")[0]
    return {"schema_version": 2, "purpose": "supplied_SOURCE_owned_first_writer_historical_relation",
            "valid_source_input": True, "historical_declared_bound_view_known": True,
            "origin": "COM_mutable_table", "recording_join_known": True,
            "execute_arguments_join_known": True, "auxiliary_arguments_consistent": True,
            "declared_view": {"resource": copy.deepcopy(srv["args"]["resource"]),
                              "original_view_call": srv["id"], "history_version": srv["id"],
                              "lineage_calls": [srv["id"]],
                              "selected_slot": {"heap": heaps[1], "index": 0, "cpu_handle": 0x41000},
                              "root_layout_call": rows(data, "RootParse")[0]["id"], "root_parameter_index": 0,
                              "format": 28, "most_detailed_mip": 0, "mip_count": 1,
                              "declared_width": 8, "declared_height": 8, "binding_origin": "COM_mutable_table",
                              "cut": rows(data, "Execute")[-1]["entry"]},
            "auxiliary_arguments": {"sampler_heap": heaps[2], "sampler_table_call": table["id"],
                                    "sampler_parameter": 1, "sampler_gpu_handle": 0x51000,
                                    "constants_call": const["id"], "constant_words": [0x3e000000, 0xbe000000],
                                    "rtv_heap": heaps[3], "rtv_resource": target,
                                    "create_rtv_call": rows(data, "CreateRTV")[0]["id"],
                                    "rtv_bind_call": rows(data, "RTV")[0]["id"]},
            "recorded_viewport": {"domain": "D3D12_VIEWPORT", "value": copy.deepcopy(rows(data, "Viewports")[0]["args"]["viewports"]["values"][0])},
            "recorded_scissor": {"domain": "D3D12_RECT", "rect": copy.deepcopy(rows(data, "Scissors")[0]["args"]["rects"]["values"][0])},
            "diagnostics": [], "claims": dict.fromkeys(CLAIMS, False), "retained_input": copy.deepcopy(data),
            "native_raster_arguments": {"viewport_call": viewport["id"], "scissor_call": scissor["id"],
                                        "viewport": copy.deepcopy(viewport["args"]["viewports"]["values"][0]),
                                        "scissor": copy.deepcopy(scissor["args"]["rects"]["values"][0])}}


def lazy_history():
    d = supplied_history()
    psoref = copy.deepcopy(rows(d, "PSOCreate")[0]["source_after"]["births"][0]["ref"])
    psoid = rows(d, "PSOCreate")[0]["id"]
    natives = [r for r in d["calls"] if r["parent"] == psoid]
    for row in natives:
        d["calls"].remove(row)
    lookup = rows(d, "PipelineLookup")[0]
    late = copy.deepcopy(lookup)
    late["id"] = max(r["id"] for r in d["calls"]) + 101
    late["source_before"]["source_site"] = "pipeline_get_or_create"
    lookup["result"] = {"value": 0, "dynamic_flags": None}
    at = d["calls"].index(lookup) + 1
    d["calls"][at:at] = [late] + natives
    for row in natives:
        row["parent"] = late["id"]
        row["source_before"] = scope(copy.deepcopy(late["source_before"]["command"]),
                                     late["source_before"]["recording_epoch_ref"], 2,
                                     pipeline_owner=psoref,
                                     pipeline_route="legacy_monolithic" if row["api"] == "PipelineCreate" else None)
        for birth in row["source_after"]["births"]:
            birth["parent"] = copy.deepcopy(psoref)
            birth["owner_creator_call"] = None
    return reclock(d)


class OwnedWriterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = importlib.import_module("tools.owned_source_writer_validate")

    def audit(self, data):
        return self.model.analyze(encoded(data))

    def assert_false_claims(self, report):
        self.assertEqual(set(report["claims"]), CLAIMS)
        self.assertTrue(all(type(v) is bool and v is False for v in report["claims"].values()))

    def unknown(self, data):
        result = self.audit(data)
        self.assertIs(result["historical_declared_bound_view_known"], False)
        self.assert_false_claims(result)
        self.assertEqual(result["retained_input"], data)
        return result

    def test_cold_complete_supplied_history(self):
        d = supplied_history()
        self.assertEqual((len(d["calls"]), d["capture"]["clock"]), (61, 122))
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20000)
        self.assertEqual((r["declared_view"]["declared_width"], r["declared_view"]["declared_height"]), (8, 8))
        self.assert_false_claims(r)
        self.assertEqual(r["retained_input"], d)

    def test_optional_cold_copy_after_close(self):
        r = self.audit(supplied_history(late=True))
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20200)
        self.assert_false_claims(r)

    def test_bytes_and_text_whole_report_equal(self):
        d = supplied_history()
        self.assertEqual(self.model.analyze(encoded(d)), self.model.analyze(encoded(d).decode()))

    def test_entire_positive_report_matches_independent_expected_value(self):
        d = supplied_history()
        self.assertEqual(self.audit(d), expected_cold_report(d))

    def test_internal_canonical_device_is_not_COM_receiver(self):
        d = supplied_history()
        dc = rows(d, "DeviceCreate")[0]
        self.assertNotEqual(dc["result"]["output_raw"], rows(d, "ResourceCreate")[0]["args"]["device"]["raw"])
        dc["source_after"]["device_interface_pointer"] += 8
        self.unknown(d)

    def test_wrong_metadata_parent_cannot_replace_actual_COM_receiver(self):
        for api in ("QueueCreate", "AllocatorCreate", "CommandCreate", "ResourceCreate", "HeapCreate", "RootCreate", "PSOCreate"):
            with self.subTest(api=api):
                d = supplied_history()
                rows(d, api)[0]["args"]["device"]["raw"] = 0x1100
                self.unknown(d)

    def test_native_device_is_separate(self):
        d = supplied_history()
        rows(d, "PipelineCreate")[0]["args"]["device"]["raw"] = 0x2180
        self.unknown(d)

    def test_canonical_parent_is_not_COM_alias_metadata(self):
        d = supplied_history()
        rows(d, "ResourceCreate")[0]["source_after"]["births"][0]["parent"]["raw"] = 0x2180
        self.unknown(d)

    def test_pending_native_creator_is_real_not_arbitrary_enclosing_call(self):
        for change in ("wrong_before", "wrong_birth", "missing_parent"):
            d = supplied_history()
            native = rows(d, "VkDeviceCreate")[0]
            if change == "wrong_before":
                native["source_before"]["owner_creation_call"] = rows(d, "RootCreate")[0]["id"]
            elif change == "wrong_birth":
                native["source_after"]["births"][0]["owner_creator_call"] = rows(d, "Draw")[0]["id"]
            else:
                native["parent"] = None
            with self.subTest(change=change):
                self.unknown(d)

    def test_constructor_Reset_is_a_real_pending_child(self):
        d = supplied_history()
        cr = rows(d, "Reset")[0]
        self.assertEqual(cr["parent"], rows(d, "CommandCreate")[0]["id"])
        self.assertIsNone(cr["args"]["command"]["generation"])
        cr["source_before"]["owner_creation_call"] = None
        self.unknown(d)

    def test_missing_constructor_Reset(self):
        d = supplied_history()
        d["calls"].remove(rows(d, "Reset")[0])
        self.unknown(reclock(d))

    def test_constructor_cannot_publish_parent_early(self):
        d = supplied_history()
        cr = rows(d, "Reset")[0]
        cr["args"]["command"]["generation"] = 17
        cr["source_before"]["command"]["generation"] = 17
        self.unknown(d)

    def test_Reset_CallID_not_ordinal_or_APP_epoch(self):
        for replacement in (1, 2, 223):
            with self.subTest(replacement=replacement):
                d = supplied_history()
                d["ticket"]["reset_call"] = replacement
                self.unknown(d)

    def test_warm_second_selected_epoch_is_unknown(self):
        d = supplied_history()
        selected = rows(d, "Reset")[1]
        selected["source_after"]["reset_ordinal"] = 3
        for c in d["calls"]:
            if c["source_before"]["reset_ordinal"] == 2:
                c["source_before"]["reset_ordinal"] = 3
        self.unknown(d)

    def test_six_resources_are_not_interchangeable(self):
        for index, field, value in ((1, "layout", 0), (3, "format", 28), (4, "format", 28), (5, "dimension", 3)):
            with self.subTest(index=index):
                d = supplied_history()
                rows(d, "ResourceCreate")[index]["args"]["description"][field] = value
                self.unknown(d)

    def test_heap_properties_fields_not_subject_to_invented_zero_guards(self):
        # Parent found an extra equality guard absent from the public contract.
        # These remain supplied observations, not a resource capability assertion.
        d = supplied_history()
        for c in rows(d, "ResourceCreate"):
            c["args"]["heap_properties"].update(cpu_page_property=2, memory_pool_preference=1,
                                                creation_node_mask=1, visible_node_mask=1)
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["retained_input"], d)
        self.assert_false_claims(r)

    def test_disjoint_creation_returns_can_reverse(self):
        d = supplied_history()
        first, second = rows(d, "ResourceCreate")[:2]
        old_first_return, old_second_entry, old_second_return = first["return"], second["entry"], second["return"]
        first["return"] = old_second_return
        second["entry"] = old_first_return
        second["return"] = old_second_entry
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20000)

    def test_four_heap_classification_and_order(self):
        d = supplied_history()
        rows(d, "SetHeaps")[0]["args"]["heaps"]["values"].reverse()
        self.unknown(d)

    def test_missing_sampler_and_RTV_originals(self):
        for api in ("CreateSampler", "CreateRTV"):
            with self.subTest(api=api):
                d = supplied_history()
                d["calls"].remove(rows(d, api)[0])
                self.unknown(reclock(d))

    def test_sampler_original_declared_values(self):
        for field in ("filter", "address_v", "comparison_func", "max_anisotropy", "max_lod_bits"):
            with self.subTest(field=field):
                d = supplied_history()
                rows(d, "CreateSampler")[0]["args"]["description"][field] += 1
                self.unknown(d)

    def test_RTV_null_original_is_not_default_typed_desc(self):
        d = supplied_history()
        rows(d, "CreateRTV")[0]["args"]["description_pointer"] = 0x1000
        self.unknown(d)

    def test_RTV_bool_is_I32_not_JSON_bool(self):
        d = supplied_history()
        rows(d, "RTV")[0]["args"]["single_range"] = True
        with self.assertRaises(ValueError):
            self.audit(d)

    def test_RTV_target_and_original_NULL_DSV(self):
        for field in ("dsv_pointer", "single_range"):
            d = supplied_history()
            rows(d, "RTV")[0]["args"][field] = 0x1234
            self.unknown(d)

    def test_getter_zero_vs_unread_mismatch(self):
        for field, value in (("value", 0), ("returned_pointer", 0)):
            with self.subTest(field=field):
                d = supplied_history()
                g = rows(d, "CPUStart")[0]
                g["result"][field] = value
                if field == "returned_pointer":
                    g["result"]["value"] = None
                self.unknown(d)

    def test_getter_ambiguity_even_equal(self):
        d = supplied_history()
        g = copy.deepcopy(rows(d, "CPUStart")[0])
        g["id"] = 99991
        d["calls"].insert(d["calls"].index(rows(d, "CPUStart")[0]), g)
        self.unknown(reclock(d))

    def test_checked_spans_and_CPU_aliases(self):
        for change in ("zero_stride", "overflow", "alias", "GPU_tag"):
            d = supplied_history()
            if change == "zero_stride":
                rows(d, "Increment")[0]["result"]["value"] = 0
            elif change == "overflow":
                rows(d, "CPUStart")[0]["result"]["value"] = 2**64 - 1
            elif change == "alias":
                rows(d, "CPUStart")[2]["result"]["value"] = 0x40000
            else:
                rows(d, "SetTable")[0]["args"]["gpu_handle"] |= 1 << 63
            with self.subTest(change=change):
                self.unknown(d)

    def test_CPU_GPU_numeric_overlap_is_not_alias(self):
        d = supplied_history()
        # This is a separate domain; preserve every original GPU operand coherently.
        for c in rows(d, "GPUStart"):
            c["result"]["value"] -= 0x10000
        for c in rows(d, "SetTable"):
            c["args"]["gpu_handle"] -= 0x10000
        self.assertIs(self.audit(d)["historical_declared_bound_view_known"], True)

    def test_root_exact_unions_and_original_legacy_flags(self):
        for change in ("sampler_type", "visibility", "version", "explicit_flags", "inactive_union"):
            d = supplied_history()
            desc = rows(d, "RootParse")[0]["result"]["description"]
            ps = desc["parameters"]["values"]
            if change == "sampler_type":
                ps[1]["ranges"]["values"][0]["range_type"] = 0
            elif change == "visibility":
                ps[2]["visibility"] = 2
            elif change == "version":
                desc["original_version"] = 2
            elif change == "explicit_flags":
                ps[0]["ranges"]["values"][0]["flags"] = 3
            else:
                ps[0]["constants"] = copy.deepcopy(ps[2]["constants"])
            with self.subTest(change=change):
                self.unknown(d)

    def test_constants_coherent_changes_are_historical_only(self):
        d = supplied_history()
        rows(d, "SetConstants")[0]["args"]["words"]["values"] = [0, 0x3f800000]
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["auxiliary_arguments"]["constant_words"], [0, 0x3f800000])
        self.assert_false_claims(r)

    def test_constants_partial_or_wrong_destination(self):
        for change in ("count", "pointer", "parameter", "destination_offset"):
            d = supplied_history()
            a = rows(d, "SetConstants")[0]["args"]
            if change == "count":
                a["words"]["count"] = 1
                a["words"]["values"] = a["words"]["values"][:1]
            elif change == "pointer":
                a["words"]["pointer"] = 0
            else:
                a[change] = 1
            with self.subTest(change=change):
                self.unknown(d)

    def test_same_root_preserves_tables_but_null_or_ClearState_invalidates(self):
        for change in ("same", "null", "clear"):
            d = supplied_history()
            base = rows(d, "SetRoot")[0]
            if change == "clear":
                row = new_row(d, "ClearState", {"command": base["args"]["command"], "pipeline": None}, source=base["source_before"])
            else:
                row = new_row(d, "SetRoot", {"command": base["args"]["command"],
                              "root": base["args"]["root"] if change == "same" else None}, source=base["source_before"])
            insert_before(d, rows(d, "Draw")[0], row)
            with self.subTest(change=change):
                if change == "same":
                    self.assertIs(self.audit(d)["historical_declared_bound_view_known"], True)
                else:
                    self.unknown(d)

    def test_extra_same_raw_setter_cannot_hide_wrong_generation(self):
        for generation in (None, 999):
            d = supplied_history()
            old = rows(d, "SetTable")[0]
            row = new_row(d, "SetTable", old["args"], source=old["source_before"])
            row["args"]["command"]["generation"] = generation
            row["source_before"]["command"]["generation"] = generation
            row["args"]["gpu_handle"] += 32
            insert_before(d, rows(d, "Draw")[0], row)
            with self.subTest(generation=generation):
                self.unknown(d)

    def test_missing_actual_emission_despite_Draw_return(self):
        d = supplied_history()
        d["calls"].remove(rows(d, "EmitDraw")[0])
        self.unknown(reclock(d))

    def test_all_four_Draw_Emit_arguments_match(self):
        for field in ("vertex_count", "instance_count", "first_vertex", "first_instance"):
            d = supplied_history()
            rows(d, "EmitDraw")[0]["args"][field] += 1
            with self.subTest(field=field):
                self.unknown(d)

    def test_coherent_zero_counts_do_not_claim_pixels(self):
        d = supplied_history()
        for api in ("Draw", "EmitDraw"):
            rows(d, api)[0]["args"].update(vertex_count=0, instance_count=0)
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assert_false_claims(r)

    def test_native_PSOwner_scope_is_required(self):
        for api in ("PipelineLookup", "BindPipeline", "EmitDraw", "VkViewportCount", "VkScissorCount"):
            d = supplied_history()
            rows(d, api)[0]["source_before"]["pipeline_owner"]["raw"] += 8
            with self.subTest(api=api):
                self.unknown(d)

    def test_internal_pso_root_and_lookup_are_not_COMraw(self):
        for change in ("lookup", "root"):
            d = supplied_history()
            if change == "lookup":
                rows(d, "PipelineLookup")[0]["args"]["state_pointer"] = 0x8800
            else:
                rows(d, "PSOCreate")[0]["source_after"]["pso_root_pointer"] = 0x7700
            self.unknown(d)

    def test_missing_native_birth_or_failed_output(self):
        for api in ("ModuleCreate", "PipelineLayoutCreate", "PipelineCreate"):
            d = supplied_history()
            r = rows(d, api)[0]
            r["result"]["vk_result"] = -1
            r["result"]["output_raw"] = None
            r["source_after"]["births"] = []
            with self.subTest(api=api):
                self.unknown(d)

    def test_failed_creators_cannot_publish_internal_associations(self):
        # Author/parent discovered this overlapping family after checkpoint-1.
        for api, field in (("PSOCreate", "pso_pointer"), ("RootCreate", "root_signature_pointer")):
            d = supplied_history()
            original = rows(d, api)[0]
            failed = new_row(d, api, original["args"], {"hresult": -1, "output_raw": None})
            insert_before(d, rows(d, "Execute")[-1], failed)
            with self.subTest(api=api, state="empty_failed_after"):
                self.assertIs(self.audit(d)["historical_declared_bound_view_known"], True)
            failed["source_after"][field] = 0x12340000
            with self.subTest(api=api, state="false_defined_after"):
                self.unknown(d)

    def test_module_destroy_after_create_not_required_at_execute(self):
        d = supplied_history()
        self.assertLess(rows(d, "PipelineCreate")[0]["return"], rows(d, "ModuleDestroy")[0]["entry"])
        self.assertIs(self.audit(d)["historical_declared_bound_view_known"], True)

    def test_genuine_lazy_birth_inside_second_lookup(self):
        d = lazy_history()
        self.assertEqual(len(rows(d, "PipelineLookup")), 2)
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20000)

    def test_lazy_cached_return_without_retained_birth_is_unknown(self):
        d = lazy_history()
        natives = {r["id"] for r in rows(d, "ModuleCreate") + rows(d, "ModuleDestroy") + rows(d, "PipelineCreate")}
        d["calls"] = [r for r in d["calls"] if r["id"] not in natives]
        self.unknown(reclock(d))

    def test_module_retirement_overlaps_native_creation(self):
        d = supplied_history()
        native = rows(d, "PipelineCreate")[0]
        destroy = rows(d, "ModuleDestroy")[0]
        # Preserve every mark but swap that module's destroyENTRY and PipelineCreateRETURN.
        native["return"], destroy["entry"] = destroy["entry"], native["return"]
        self.unknown(d)

    def test_unsupported_native_pipeline_branches_not_monolithic(self):
        for route in ("descriptor_heap", "GPL_link", "other"):
            d = supplied_history()
            rows(d, "PipelineCreate")[0]["source_before"]["pipeline_route"] = route
            with self.subTest(route=route):
                self.unknown(d)

    def test_native_layout_or_shader_stage_mismatch(self):
        for change in ("layout", "stage", "entry", "pointer"):
            d = supplied_history()
            a = rows(d, "PipelineCreate")[0]["args"]
            if change == "layout":
                a["layout"]["raw"] += 8
            elif change == "stage":
                a["stages"]["values"][1]["stage"] = 32
            elif change == "entry":
                a["stages"]["values"][0]["entry"] = "not_main"
            else:
                a["create_info_pointer"] = 0
            with self.subTest(change=change):
                self.unknown(d)

    def test_original_flags2_chain_not_default_zero(self):
        d = supplied_history()
        chain = rows(d, "PipelineCreate")[0]["args"]["p_next"]
        node = {"kind": "flags2", "pointer": 0x150000, "s_type": 1000470005,
                "next_pointer": 0, "payload": {"flags": 0}}
        chain["nodes"][-1]["next_pointer"] = node["pointer"]
        chain["nodes"].append(node)
        self.unknown(d)

    def test_native_raster_missing_or_wrong_scope(self):
        for api in ("VkViewportCount", "VkScissorCount"):
            d = supplied_history()
            rows(d, api)[0]["source_before"]["recording_epoch_ref"] = None
            with self.subTest(api=api):
                self.unknown(d)

    def test_original_raster_arrays_do_not_use_nonexistent_first(self):
        d = supplied_history()
        rows(d, "VkViewportCount")[0]["args"]["first"] = 0
        with self.assertRaises(ValueError):
            self.audit(d)

    def test_void_return_is_not_pending_or_HRESULT(self):
        d = supplied_history()
        rows(d, "CreateSRV")[0]["result"] = {"hresult": 0}
        with self.assertRaises(ValueError):
            self.audit(d)

    def test_pending_fault_and_off_are_unknown(self):
        for field in ("faulted", "gap", "overflow", "enabled"):
            d = supplied_history()
            d["capture"][field] = field != "enabled"
            with self.subTest(field=field):
                self.unknown(d)

    def test_pending_Execute_has_no_fabricated_void_return(self):
        d = supplied_history()
        execute = rows(d, "Execute")[-1]
        execute["return"] = None
        d["capture"].update(pending=1, original_returns=60, clock=121, sealed_at=121)
        self.unknown(d)

    def test_Release_zero_or_nonfinal_ends_selected_eligibility(self):
        for api, count in (("HeapCreate", 0), ("HeapCreate", 1), ("ResourceCreate", 1), ("DeviceCreate", 1)):
            d = supplied_history()
            obj = copy.deepcopy(rows(d, api)[0]["source_after"]["births"][0]["ref"])
            if api == "DeviceCreate":
                obj["raw"] = 0x2180  # Release is the original COM receiver, not metadata-parent raw.
            row = new_row(d, "Release", {"object": obj}, {"value": count})
            insert_before(d, rows(d, "Execute")[-1], row)
            with self.subTest(api=api, count=count):
                self.unknown(d)

    def test_COM_Release_cannot_be_native_destroy_even_after_cut(self):
        # Parent found that an after-cut callback was escaping COM-kind validation.
        for api in ("VkDeviceCreate", "PipelineCreate"):
            d = supplied_history()
            obj = rows(d, api)[0]["source_after"]["births"][0]["ref"]
            row = new_row(d, "Release", {"object": obj}, {"value": 1})
            concurrent_before_return(d, rows(d, "Execute")[-1], row)
            with self.subTest(native_kind=obj["kind"]):
                self.unknown(d)

    def test_native_retirement_requires_owner_without_moving_execute_cut(self):
        # Author/parent finding: applicability still applies after the historical cut.
        d = supplied_history()
        pipeline = rows(d, "PipelineCreate")[0]["source_after"]["births"][0]["ref"]
        pso = rows(d, "PSOCreate")[0]["source_after"]["births"][0]["ref"]
        device = rows(d, "VkDeviceCreate")[0]["source_after"]["births"][0]["ref"]
        row = new_row(d, "PipelineDestroy", {"device": device, "object": pipeline,
                                             "allocator_pointer": 0},
                      source=scope(pipeline_owner=pso), domain="Vulkan_original")
        concurrent_before_return(d, rows(d, "Execute")[-1], row)
        self.assertIs(self.audit(d)["historical_declared_bound_view_known"], True)
        row["source_before"]["pipeline_owner"] = None
        self.unknown(d)

    def test_reused_raw_does_not_retire_old_nonfinal_generation(self):
        d = supplied_history()
        resource = rows(d, "ResourceCreate")[0]
        duplicate = copy.deepcopy(resource)
        duplicate["id"] = 99997
        duplicate["source_after"]["births"][0]["ref"]["generation"] = 100
        insert_before(d, rows(d, "CreateSRV")[0], duplicate)
        self.unknown(d)

    def test_contiguous_clock_no_offset_or_duplicate(self):
        for change in ("offset", "duplicate", "sealed"):
            d = supplied_history()
            if change == "offset":
                for c in d["calls"]:
                    c["entry"] += 100
                    c["return"] += 100
                d["capture"]["clock"] += 100
                d["capture"]["sealed_at"] += 100
            elif change == "duplicate":
                d["calls"][1]["entry"] = d["calls"][0]["entry"]
            else:
                d["capture"]["sealed_at"] -= 1
            with self.subTest(change=change):
                self.unknown(d)

    def test_copy_ranges_NULL_sizes_are_original_unit_ranges(self):
        d = supplied_history(late=True)
        c = rows(d, "CopySimple")[0]
        c["api"] = "CopyRanges"
        c["args"] = {"device": c["args"]["device"], "heap_type": 0,
                     "sources": {"count": 1, "starts_pointer": 0x151000, "starts": [0x40020], "sizes_pointer": 0, "sizes": None},
                     "destinations": {"count": 1, "starts_pointer": 0x151100, "starts": [0x41000], "sizes_pointer": 0, "sizes": None}}
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20200)

    def test_copy_source_is_frozen_at_ENTRY_not_later_RETURN(self):
        d = supplied_history(late=True)
        original = rows(d, "CreateSRV")[0]
        a = copy.deepcopy(original["args"])
        a["destination"] = 0x40020  # Replace CPU B by A while the already-entered copy is pending.
        row = new_row(d, "CreateSRV", a)
        concurrent_before_return(d, rows(d, "CopySimple")[0], row)
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20200)

    def test_overlapping_destination_writes_do_not_choose_last_RETURN(self):
        d = supplied_history(late=True)
        a = copy.deepcopy(rows(d, "CreateSRV")[-1]["args"])
        row = new_row(d, "CreateSRV", a)
        concurrent_before_return(d, rows(d, "CopySimple")[0], row)
        self.unknown(d)

    def test_post_Execute_ENTRY_mutation_does_not_rewrite_selected_cut(self):
        d = supplied_history()
        a = copy.deepcopy(rows(d, "CreateSRV")[1]["args"])
        a["destination"] = 0x41000
        row = new_row(d, "CreateSRV", a)
        concurrent_before_return(d, rows(d, "Execute")[-1], row)
        r = self.audit(d)
        self.assertIs(r["historical_declared_bound_view_known"], True)
        self.assertEqual(r["declared_view"]["resource"]["raw"], 0x20000)

    def test_wrong_copy_direction_and_heap_type(self):
        for change in ("direction", "type", "count"):
            d = supplied_history(late=True)
            a = rows(d, "CopySimple")[0]["args"]
            if change == "direction":
                a["source"], a["destination"] = a["destination"], a["source"]
            else:
                a["heap_type" if change == "type" else "count"] = 1 if change == "type" else 2
            with self.subTest(change=change):
                self.unknown(d)

    def test_scalar_no_bool_float_and_U64_overflow(self):
        for bad in (True, 1.0, -1, 2**64):
            d = supplied_history()
            rows(d, "CreateSRV")[0]["args"]["destination"] = bad
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.audit(d)

    def test_nonstrings_API_kind_and_closed_enum_public_errors(self):
        for bad in ([], {}):
            for field in ("api", "kind", "source_site"):
                d = supplied_history()
                if field == "api":
                    d["calls"][0]["api"] = bad
                elif field == "kind":
                    rows(d, "ResourceCreate")[0]["args"]["device"]["kind"] = bad
                else:
                    rows(d, "Draw")[0]["source_before"]["source_site"] = bad
                with self.subTest(field=field, bad=bad), self.assertRaises(ValueError):
                    self.audit(d)

    def test_strict_enum_flags_and_capture_boolean_domains(self):
        for change in ("stage", "bind_point", "flags", "enabled", "run_id"):
            d = supplied_history()
            if change == "stage":
                rows(d, "PipelineCreate")[0]["args"]["stages"]["values"][0]["stage"] = True
            elif change == "bind_point":
                rows(d, "BindPipeline")[0]["args"]["bind_point"] = 0.0
            elif change == "flags":
                rows(d, "ResourceCreate")[0]["args"]["description"]["flags"] = -1
            elif change == "enabled":
                d["capture"]["enabled"] = 1
            else:
                d["capture"]["run_id"] = "G" * 32
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.audit(d)

    def test_prefix_beyond_actual_count_is_malformed(self):
        d = supplied_history()
        rows(d, "Viewports")[0]["args"]["viewports"]["count"] = 0
        with self.assertRaises(ValueError):
            self.audit(d)

    def test_original_null_array_is_not_invented(self):
        d = supplied_history()
        rows(d, "SetHeaps")[0]["args"]["heaps"]["pointer"] = 0
        self.unknown(d)

    def test_strict_raw_JSON_and_public_API(self):
        for raw in (b'{"schema_version":2,"schema_version":2}', b'\xff', b'NaN', b'[]', '{}'.encode('utf-16')):
            with self.subTest(raw=raw[:20]), self.assertRaises(ValueError):
                self.model.analyze(raw)
        with self.assertRaises(ValueError):
            self.model.analyze(supplied_history())
        raw = encoded(supplied_history()).replace(b'"schema_version":2', b'"schema_version":2,"schema_version":2', 1)
        with self.assertRaises(ValueError):
            self.model.analyze(raw)

    def test_input_raw_budget_not_whitespace_normalization(self):
        raw = b' ' * 65537 + encoded(supplied_history())
        with self.assertRaises(ValueError):
            self.model.analyze(raw)

    def test_nodes_depth_calls_and_pending_caps(self):
        for field, value in (("pending", 33),):
            d = supplied_history()
            d["capture"][field] = value
            with self.assertRaises(ValueError):
                self.audit(d)
        d = supplied_history()
        d["calls"].extend(copy.deepcopy(d["calls"][:4]))
        with self.assertRaises(ValueError):
            self.audit(d)
        for extra in ([0] * 4097, [[[[[[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]]]]]]):
            d = supplied_history()
            d["extra"] = extra
            with self.assertRaises(ValueError):
                self.audit(d)

    def test_whole_output_budget_refuses_without_truncation(self):
        with mock.patch.object(self.model, "MAX_OUTPUT_BYTES", 128):
            with self.assertRaises(ValueError):
                self.audit(supplied_history())

    def test_whole_report_fresh_and_80_input_churn(self):
        d = supplied_history()
        expected = self.audit(d)
        held = copy.deepcopy(expected)
        for i in range(80):
            fresh = supplied_history()
            fresh["capture"]["run_id"] = f"{i:032x}"
            result = self.audit(fresh)
            self.assertIs(result["historical_declared_bound_view_known"], True)
            result["retained_input"]["calls"].clear()
            self.assertEqual(expected, held)
        self.assertEqual(self.audit(d), held)

    def test_silent_overwrite_observational_twins(self):
        # Hidden descriptor or texture writes are absent from both supplied histories.
        left, right = supplied_history(), supplied_history()
        self.assertEqual(self.audit(left), self.audit(right))
        self.assert_false_claims(self.audit(left))


if __name__ == "__main__":
    unittest.main()
