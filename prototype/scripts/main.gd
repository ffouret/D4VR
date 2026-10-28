extends Node3D

const Rig = preload("res://scripts/camera_rig.gd")
const Sanctuary = preload("res://scripts/sanctuary.gd")
const SCALES: Array[float] = [1.0, 4.0, 8.0, 12.0]

var rig := Rig.new()
var origin: XROrigin3D
var head: XRCamera3D
var desktop: Camera3D
var left: XRController3D
var right: XRController3D
var player: CharacterBody3D
var hud: Label
var status_board: Label3D
var xr: OpenXRInterface
var vr_mode: bool = false
var session_focused: bool = false
var calibrated: bool = false
var following: bool = true
var fixed_anchor: Vector3 = Vector3.ZERO
var snap_ready: bool = true
var scale_index: int = 2
var hud_clock: float = 0.0


func _ready() -> void:
	vr_mode = not "--desktop" in OS.get_cmdline_user_args()
	var scene := Sanctuary.new()
	scene.name = "SyntheticSanctuary"
	add_child(scene)
	player = scene.create_player()
	add_child(player)
	origin = XROrigin3D.new()
	origin.name = "TrackingOrigin"
	origin.world_scale = rig.world_scale
	add_child(origin)
	origin.current = true
	head = XRCamera3D.new()
	head.name = "TrackedHead"
	head.near = 0.05
	head.far = 250.0
	origin.add_child(head)
	left = _controller("left_hand")
	right = _controller("right_hand")
	left.button_pressed.connect(_on_button)
	right.button_pressed.connect(_on_button)
	desktop = Camera3D.new()
	desktop.fov = 65.0
	add_child(desktop)
	if vr_mode:
		xr = XRServer.find_interface("OpenXR") as OpenXRInterface
		if xr == null or not xr.is_initialized():
			push_error("D4VR: OpenXR indisponible. Démarrer SteamVR et connecter le casque. Utiliser --desktop pour l'aperçu explicite.")
			get_tree().quit(2)
			return
		get_viewport().use_xr = true
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		head.current = true
		xr.session_focussed.connect(func() -> void: session_focused = true)
		xr.session_visible.connect(func() -> void: session_focused = false)
		xr.session_stopping.connect(func() -> void: session_focused = false)
		xr.session_loss_pending.connect(func() -> void: session_focused = false)
		xr.pose_recentered.connect(func() -> void: calibrated = false)
		xr.instance_exiting.connect(func() -> void: get_tree().quit())
		session_focused = xr.get_session_state() == OpenXRInterface.SESSION_STATE_FOCUSED
		status_board = Label3D.new()
		status_board.font_size = 44
		status_board.pixel_size = 0.007
		status_board.no_depth_test = false
		add_child(status_board)
	else:
		desktop.current = true
		_make_desktop_hud()
	_update_view()
	print("D4VR lab 0.1.0 | mode=", "OpenXR" if vr_mode else "desktop", " | source=synthetic_geometry | Diablo IV integration=absent")
	for argument: String in OS.get_cmdline_user_args():
		if argument.begins_with("--capture=") and not vr_mode:
			_capture_preview.call_deferred(argument.trim_prefix("--capture="))


func _capture_preview(path: String) -> void:
	for frame in range(8):
		await RenderingServer.frame_post_draw
	var result := get_viewport().get_texture().get_image().save_png(path)
	print("D4VR preview capture: ", path, " (", result, ")")
	get_tree().quit(0 if result == OK else 1)


func _controller(tracker: String) -> XRController3D:
	var controller := XRController3D.new()
	controller.tracker = tracker
	controller.pose = "grip"
	origin.add_child(controller)
	return controller


func _tracking_valid() -> bool:
	var tracker := XRServer.get_tracker("head") as XRPositionalTracker
	return tracker != null and tracker.has_pose("default") and tracker.get_pose("default").has_tracking_data


func _physics_process(delta: float) -> void:
	if vr_mode and session_focused and not calibrated and _tracking_valid():
		rig.recenter(head.transform)
		calibrated = true
	var movement := Vector2.ZERO
	if not vr_mode or (session_focused and calibrated and _tracking_valid()):
		movement = _keyboard_movement()
		if vr_mode:
			var stick := left.get_vector2("move")
			if stick.length() > 0.18:
				movement += Vector2(stick.x, -stick.y)
			var view := right.get_vector2("adjust_view")
			if absf(view.x) < 0.35:
				snap_ready = true
			elif absf(view.x) > 0.75 and snap_ready:
				rig.azimuth -= signf(view.x) * deg_to_rad(30)
				snap_ready = false
			if absf(view.y) > 0.35:
				rig.height_units = clampf(rig.height_units + view.y * delta * 4.0, 4.0, 24.0)
		else:
			var height_input := float(Input.is_physical_key_pressed(KEY_PAGEUP)) - float(Input.is_physical_key_pressed(KEY_PAGEDOWN))
			rig.height_units = clampf(rig.height_units + height_input * delta * 4.0, 4.0, 24.0)
	var direction := Basis(Vector3.UP, rig.azimuth) * Vector3(movement.x, 0, movement.y).limit_length()
	player.velocity = Vector3(direction.x * 4.0, -1.0, direction.z * 4.0)
	player.move_and_slide()
	player.position.x = clampf(player.position.x, -10.4, 10.4)
	player.position.z = clampf(player.position.z, -12.1, 10.4)
	if direction.length() > 0.05:
		player.rotation.y = atan2(-direction.x, -direction.z)
	_update_view()
	hud_clock += delta
	if hud_clock >= 0.25:
		hud_clock = 0.0
		_update_status()


func _update_view() -> void:
	var anchor := player.position if following else fixed_anchor
	origin.world_scale = rig.world_scale
	origin.transform = rig.origin_for(anchor)
	desktop.transform = rig.desktop_transform(anchor)
	if status_board != null:
		status_board.position = anchor + Vector3(0, 4.3, -3.8)
		if status_board.position.distance_to(head.global_position) > 0.01:
			status_board.look_at(head.global_position, Vector3.UP, true)


func _keyboard_movement() -> Vector2:
	# Physical WASD positions support both QWERTY and AZERTY; arrows also work.
	var x := float(Input.is_physical_key_pressed(KEY_D) or Input.is_physical_key_pressed(KEY_RIGHT)) - float(Input.is_physical_key_pressed(KEY_A) or Input.is_physical_key_pressed(KEY_LEFT))
	var y := float(Input.is_physical_key_pressed(KEY_S) or Input.is_physical_key_pressed(KEY_DOWN)) - float(Input.is_physical_key_pressed(KEY_W) or Input.is_physical_key_pressed(KEY_UP))
	return Vector2(x, y)


func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo:
		return
	match event.physical_keycode:
		KEY_ESCAPE:
			get_tree().quit()
		KEY_R:
			_on_button("recenter")
		KEY_TAB:
			_on_button("cycle_scale")
		KEY_F:
			_on_button("toggle_follow")
		KEY_Q:
			rig.azimuth += deg_to_rad(30)
		KEY_E:
			rig.azimuth -= deg_to_rad(30)
	_update_view()


func _on_button(action: String) -> void:
	if vr_mode and (not session_focused or not _tracking_valid()):
		return
	match action:
		"recenter":
			if vr_mode:
				rig.recenter(head.transform)
				calibrated = true
		"cycle_scale":
			scale_index = (scale_index + 1) % SCALES.size()
			rig.change_scale(SCALES[scale_index], head.transform)
		"toggle_follow":
			fixed_anchor = player.position
			following = not following


func _make_desktop_hud() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)
	var panel := PanelContainer.new()
	panel.position = Vector2(28, 28)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.025, 0.04, 0.06, 0.94)
	style.border_color = Color("6ac8bd")
	style.border_width_left = 3
	style.content_margin_left = 22
	style.content_margin_top = 18
	style.content_margin_right = 22
	style.content_margin_bottom = 18
	panel.add_theme_stylebox_override("panel", style)
	layer.add_child(panel)
	hud = Label.new()
	hud.add_theme_font_size_override("font_size", 16)
	hud.add_theme_color_override("font_color", Color("dce6ed"))
	panel.add_child(hud)
	_update_status()


func _update_status() -> void:
	var follow_text := "personnage" if following else "fixe"
	if hud != null:
		hud.text = "D4VR  /  LAB 01\nSURPLOMB STÉRÉOSCOPIQUE\n\nScène synthétique · aperçu bureau\nIntégration Diablo IV : à développer\n\nFlèches / ZQSD : déplacement\nQ / E physiques : rotation par pas\nPage ↑↓ : hauteur · Tab : échelle\nF : suivi · Échap : quitter\n\nHauteur %.1f · échelle 1:%.0f\nSuivi : %s" % [rig.height_units, rig.world_scale, follow_text]
	if status_board != null:
		var state := "ACTIF" if session_focused and calibrated else "EN ATTENTE DU TRACKING"
		status_board.text = "D4VR · %s\nSCÈNE SYNTHÉTIQUE\nA : recentrer · B : échelle 1:%.0f\nY : suivi %s · stick droit : vue" % [state, rig.world_scale, follow_text]
