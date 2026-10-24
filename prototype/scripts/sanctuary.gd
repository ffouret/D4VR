extends Node3D
## Original procedural test scene. Contains no Blizzard assets.

var stone: StandardMaterial3D
var trim: StandardMaterial3D
var dark: StandardMaterial3D
var gold: StandardMaterial3D
var cyan: StandardMaterial3D


func _ready() -> void:
	stone = material(Color("39444e"))
	trim = material(Color("667478"))
	dark = material(Color("171f29"))
	gold = material(Color("b48b48"))
	cyan = material(Color("65e3df"), 0.6)
	_build_lighting()
	_build_floor()
	_build_architecture()
	_build_markers()


func material(color: Color, glow: float = 0.0) -> StandardMaterial3D:
	var result := StandardMaterial3D.new()
	result.albedo_color = color
	result.roughness = 0.87
	if glow > 0.0:
		result.emission_enabled = true
		result.emission = color
		result.emission_energy_multiplier = glow
	return result


func box(parent: Node3D, dimensions: Vector3, at: Vector3, surface: Material, solid: bool = false) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = dimensions
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = surface
	instance.position = at
	parent.add_child(instance)
	if solid:
		var body := StaticBody3D.new()
		var collision := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = dimensions
		collision.shape = shape
		body.add_child(collision)
		instance.add_child(body)
	return instance


func cylinder(parent: Node3D, radius: float, height: float, at: Vector3, surface: Material) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 20
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = surface
	instance.position = at
	parent.add_child(instance)
	return instance


func _build_lighting() -> void:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color("0c1420")
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("92b3cf")
	environment.ambient_light_energy = 0.65
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-54, -35, 0)
	sun.light_color = Color("c5dbef")
	sun.light_energy = 1.5
	sun.shadow_enabled = true
	add_child(sun)
	for x in [-8.5, 8.5]:
		for z in [-8.0, 5.0]:
			var fire := OmniLight3D.new()
			fire.position = Vector3(x, 2.8, z)
			fire.light_color = Color("ffb065")
			fire.light_energy = 4.0
			fire.omni_range = 7.0
			add_child(fire)
			cylinder(self, 0.17, 2.5, Vector3(x, 1.25, z), gold)
			var flame := SphereMesh.new()
			flame.radius = 0.22
			flame.height = 0.65
			var orb := MeshInstance3D.new()
			orb.mesh = flame
			orb.material_override = material(Color("ffb86a"), 2.0)
			orb.position = fire.position
			add_child(orb)


func _build_floor() -> void:
	box(self, Vector3(24, 0.6, 26), Vector3(0, -0.3, -1), dark, true)
	# Batched stone tiles keep the native stereo scene inexpensive to render.
	var mesh := BoxMesh.new()
	mesh.size = Vector3(1.94, 0.16, 1.94)
	var tiles := MultiMesh.new()
	tiles.transform_format = MultiMesh.TRANSFORM_3D
	tiles.use_colors = true
	tiles.mesh = mesh
	tiles.instance_count = 12 * 13
	for x in range(12):
		for z in range(13):
			var i := x * 13 + z
			tiles.set_instance_transform(i, Transform3D(Basis.IDENTITY, Vector3(x * 2 - 11, -0.02, z * 2 - 13)))
			var shade := 0.26 + float((x * 7 + z * 3) % 5) * 0.018
			tiles.set_instance_color(i, Color(shade * 0.9, shade, shade * 1.12))
	var surface := material(Color.WHITE)
	surface.vertex_color_use_as_albedo = true
	var field := MultiMeshInstance3D.new()
	field.multimesh = tiles
	field.material_override = surface
	add_child(field)
	for x in [-2.4, 2.4]:
		box(self, Vector3(0.08, 0.035, 21), Vector3(x, 0.09, -1), gold)
	for radius in [2.0, 2.25]:
		var ring := TorusMesh.new()
		ring.inner_radius = radius - 0.035
		ring.outer_radius = radius + 0.035
		var instance := MeshInstance3D.new()
		instance.mesh = ring
		instance.position = Vector3(0, 0.12, -2)
		instance.material_override = gold
		add_child(instance)


func _build_architecture() -> void:
	for x in [-11.5, 11.5]:
		box(self, Vector3(0.8, 1.6, 26), Vector3(x, 0.8, -1), stone, true)
	for x in [-7.5, 7.5]:
		box(self, Vector3(7, 3.0, 0.8), Vector3(x, 1.5, -13), stone, true)
	for x in [-7.0, 7.0]:
		for z in [-9.0, -2.0, 5.0]:
			var pillar := Node3D.new()
			pillar.position = Vector3(x, 0, z)
			add_child(pillar)
			box(pillar, Vector3(1.5, 0.4, 1.5), Vector3(0, 0.2, 0), trim, true)
			box(pillar, Vector3(0.9, 4.8, 0.9), Vector3(0, 2.8, 0), stone, true)
			box(pillar, Vector3(1.35, 0.3, 1.35), Vector3(0, 5.35, 0), trim)
			box(pillar, Vector3(1.0, 0.08, 1.0), Vector3(0, 5.54, 0), gold)
	# A freestanding arch provides changing occlusion when the viewer leans.
	for x in [-3.5, 3.5]:
		box(self, Vector3(0.9, 4.6, 1.2), Vector3(x, 2.3, -10.5), trim, true)
	for index in range(11):
		var angle := float(index) / 10.0 * PI
		var block := box(self, Vector3(0.85, 1.0, 1.2), Vector3(cos(angle) * 3.5, 4.4 + sin(angle) * 2.4, -10.5), stone)
		block.rotation.z = angle - PI / 2.0
	box(self, Vector3(3, 0.5, 2), Vector3(0, 0.25, -6.5), dark, true)
	box(self, Vector3(2.6, 0.35, 1.5), Vector3(0, 0.68, -6.5), trim, true)
	box(self, Vector3(1.8, 0.12, 0.8), Vector3(0, 0.94, -6.5), gold)


func _build_markers() -> void:
	for marker in [Vector3(-4.5, 1.0, -1.5), Vector3(4.5, 2.0, -6), Vector3(0, 3.0, -12)]:
		var mesh := SphereMesh.new()
		mesh.radius = 0.32
		mesh.height = 0.64
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.position = marker
		instance.material_override = cyan
		add_child(instance)
		cylinder(self, 0.04, marker.y, Vector3(marker.x, marker.y / 2.0, marker.z), gold)
	var plaque := Label3D.new()
	plaque.text = "D4VR / STEREO LAB\nSCÈNE SYNTHÉTIQUE"
	plaque.font_size = 42
	plaque.pixel_size = 0.009
	plaque.modulate = Color("c5d5de")
	plaque.position = Vector3(0, 3.0, -12.4)
	add_child(plaque)


func create_player() -> CharacterBody3D:
	var body := CharacterBody3D.new()
	body.name = "TestCharacter"
	var shape := CapsuleShape3D.new()
	shape.radius = 0.35
	shape.height = 1.8
	var collider := CollisionShape3D.new()
	collider.shape = shape
	collider.position.y = 0.95
	body.add_child(collider)
	var armour := material(Color("a2b4c3"))
	box(body, Vector3(0.65, 0.75, 0.45), Vector3(0, 1.1, 0), armour)
	box(body, Vector3(0.42, 0.42, 0.42), Vector3(0, 1.7, 0), trim)
	box(body, Vector3(0.7, 0.9, 0.08), Vector3(0, 1.0, 0.29), material(Color("8d393c")))
	for x in [-0.2, 0.2]:
		box(body, Vector3(0.22, 0.55, 0.25), Vector3(x, 0.4, 0), dark)
	box(body, Vector3(0.1, 1.05, 0.12), Vector3(0.55, 0.9, -0.15), armour)
	box(body, Vector3(0.42, 0.09, 0.15), Vector3(0.55, 0.95, -0.15), gold)
	cylinder(body, 0.65, 0.035, Vector3(0, 0.1, 0), cyan)
	return body
