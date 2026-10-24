extends SceneTree

const Rig = preload("res://scripts/camera_rig.gd")
var failures: int = 0


func _initialize() -> void:
	var rig := Rig.new()
	var anchor := Vector3(10, 0, -3)
	var head := Transform3D(Basis.from_euler(Vector3(-0.6, 0.8, 0.1)), Vector3(2.4, 13.6, -1.6))
	rig.recenter(head)
	var origin: Transform3D = rig.origin_for(anchor)
	var expected := anchor + Basis(Vector3.UP, rig.azimuth) * Vector3(0, 12, 14)
	_check((origin * head.origin).is_equal_approx(expected), "Recenter positions the physical head at the intended overlook")
	_check(origin.basis.y.is_equal_approx(Vector3.UP), "Tracking up stays vertical despite head pitch and roll")
	var moved_head := head
	moved_head.origin += Vector3(0.8, 0, 0)
	var before_scale := origin * moved_head.origin
	var new_anchor := anchor + Vector3(1, 0, -2)
	_check((rig.origin_for(new_anchor) * moved_head.origin - before_scale).is_equal_approx(new_anchor - anchor), "Follow translates both views by the character displacement")
	var old_scale: float = rig.world_scale
	rig.change_scale(4.0, moved_head)
	var next_head := moved_head
	next_head.origin *= rig.world_scale / old_scale
	_check((rig.origin_for(anchor) * next_head.origin).is_equal_approx(before_scale), "Changing scale preserves a displaced physical head's world position")
	_check((rig.origin_for(anchor) * (next_head.origin + Vector3(0.4, 0, 0)) - before_scale).length() > 0.39, "Leaning remains translational rather than screen rotation")
	rig.change_scale(-100.0, next_head)
	_check(is_equal_approx(rig.world_scale, 1.0), "Scale clamps to the supported lower bound")
	rig.change_scale(INF, next_head)
	_check(is_finite(rig.world_scale), "Non-finite scale is rejected")
	var preview: Transform3D = rig.desktop_transform(anchor)
	var forward := -preview.basis.z
	_check(forward.dot((anchor + Vector3(0, 0.8, 0) - preview.origin).normalized()) > 0.999, "Desktop preview aims at the character")
	var map := load("res://xr_actions.tres") as OpenXRActionMap
	_check(map != null and map.get_action_set_count() == 1, "A saved action map is available at XR startup")
	if map != null:
		var set := map.find_action_set("d4vr")
		_check(set != null and set.get_action_count() == 6, "The six declared XR actions are present")
		var touch := map.find_interaction_profile("/interaction_profiles/oculus/touch_controller")
		_check(touch != null and touch.get_binding_count() == 7, "Seven Touch input bindings survive serialization")
		if touch != null:
			for index in range(touch.get_binding_count()):
				var binding := touch.get_binding(index)
				_check(binding.action != null and binding.binding_path.begins_with("/user/hand/"), "Each serialized binding references an action and a controller input")
	print("D4VR rig tests: ", "PASS" if failures == 0 else "FAIL", " (", failures, " failures)")
	quit(0 if failures == 0 else 1)


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error(message)
