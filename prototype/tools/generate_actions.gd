extends SceneTree
## Generates a minimal, reviewable OpenXR action map for Quest Touch controllers.


func _initialize() -> void:
	var action_map := OpenXRActionMap.new()
	var action_set := OpenXRActionSet.new()
	action_set.resource_name = "d4vr"
	action_set.localized_name = "D4VR Lab"
	action_map.add_action_set(action_set)
	var touch := OpenXRInteractionProfile.new()
	touch.interaction_profile_path = "/interaction_profiles/oculus/touch_controller"
	action_map.add_interaction_profile(touch)
	_add(action_set, touch, "grip", "Controller pose", OpenXRAction.OPENXR_ACTION_POSE, ["/user/hand/left/input/grip/pose", "/user/hand/right/input/grip/pose"])
	_add(action_set, touch, "move", "Move character", OpenXRAction.OPENXR_ACTION_VECTOR2, ["/user/hand/left/input/thumbstick"])
	_add(action_set, touch, "adjust_view", "Adjust height and orbit", OpenXRAction.OPENXR_ACTION_VECTOR2, ["/user/hand/right/input/thumbstick"])
	_add(action_set, touch, "recenter", "Recenter view", OpenXRAction.OPENXR_ACTION_BOOL, ["/user/hand/right/input/a/click"])
	_add(action_set, touch, "cycle_scale", "Change world scale", OpenXRAction.OPENXR_ACTION_BOOL, ["/user/hand/right/input/b/click"])
	_add(action_set, touch, "toggle_follow", "Toggle character following", OpenXRAction.OPENXR_ACTION_BOOL, ["/user/hand/left/input/y/click"])
	assert(touch.get_binding_count() == 7, "All controller bindings must be present before saving")
	var result := ResourceSaver.save(action_map, "res://xr_actions.tres")
	if result != OK:
		push_error("Unable to save XR action map: %s" % result)
		quit(1)
	else:
		print("D4VR XR action map generated")
		quit()


func _add(action_set: OpenXRActionSet, profile: OpenXRInteractionProfile, action_name: String, label: String, kind: OpenXRAction.ActionType, paths: Array) -> void:
	var action := OpenXRAction.new()
	action.resource_name = action_name
	action.localized_name = label
	action.action_type = kind
	var hands := PackedStringArray()
	for path: String in paths:
		var hand := path.get_slice("/input/", 0)
		if not hand in hands:
			hands.append(hand)
	action.toplevel_paths = hands
	action_set.add_action(action)
	for path: String in paths:
		var binding := OpenXRIPBinding.new()
		binding.action = action
		binding.binding_path = path
		var bindings: Array = profile.bindings.duplicate()
		bindings.append(binding)
		profile.bindings = bindings
