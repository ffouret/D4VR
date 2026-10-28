extends RefCounted
## All tracking transforms are runtime-owned. This class only moves the origin.
## Scene units are metres at scale 1; scale 8 makes the scene appear 8x smaller.

var height_units: float = 12.0
var distance_units: float = 14.0
var world_scale: float = 8.0
var azimuth: float = deg_to_rad(35.0)
var neutral_tracking_metres: Vector3 = Vector3.ZERO
var neutral_yaw: float = 0.0


func recenter(head_local: Transform3D) -> void:
	neutral_tracking_metres = head_local.origin / world_scale
	var backward := head_local.basis.z
	# Looking straight down must not choose an arbitrary heading.
	if Vector2(backward.x, backward.z).length() > 0.01:
		neutral_yaw = atan2(backward.x, backward.z)


func origin_for(anchor: Vector3) -> Transform3D:
	var heading := Basis(Vector3.UP, azimuth - neutral_yaw)
	var offset := Basis(Vector3.UP, azimuth) * Vector3(0.0, height_units, distance_units)
	return Transform3D(heading, anchor + offset - heading * neutral_tracking_metres * world_scale)


func change_scale(value: float, current_head_local: Transform3D) -> void:
	if not is_finite(value):
		return
	var next_scale := clampf(value, 1.0, 16.0)
	var current_metres := current_head_local.origin / world_scale
	# Preserve the current physical head's world position across the scale change.
	neutral_tracking_metres = current_metres - (current_metres - neutral_tracking_metres) * world_scale / next_scale
	world_scale = next_scale


func desktop_transform(anchor: Vector3) -> Transform3D:
	var offset := Basis(Vector3.UP, azimuth) * Vector3(0.0, height_units, distance_units)
	var result := Transform3D(Basis.IDENTITY, anchor + offset)
	return result.looking_at(anchor + Vector3(0.0, 0.8, 0.0), Vector3.UP)
