Deep projections will project a texture on each deep sample, with relationship to the deep's world position.

1st input is the deep itself.
2nd input is the texture to project.
3rd input is the camera to project from.

4th input is optional, if no input provided it will look at the metadata for world_to_NDC and world_to_Camera,
if it fails to find either it will produce a default camera.

3 merge operation are provided to blend with the deep itself.
if the alpha of the texture is needed, you will need to shuffle it to a separate channel and shuffle back after the DeepProjection.
Hopefully in the future it will be handled automatically.

Alpha there are a few edge handling (reflection will be added in future version)

Blur will handle any pre-filtering of the texture (future version will have an adaptive mode).

Also the projection is culled by the camera's near and far clipping planes.
