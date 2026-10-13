# gvaroiscale

Finds the first valid object-detection ROI in `GstAnalyticsRelationMeta`, crops it from an
RGB frame, preserves aspect ratio, and centers the resized image on a black canvas. Frames
without a valid ROI are dropped.

| Property | Type | Range | Default | Description |
|---|---|---|---|---|
| `out-width` | Unsigned Integer | [1, 8192] | 448 | Output width; configure to match the VLM model input. |
| `out-height` | Unsigned Integer | [1, 8192] | 448 | Output height; configure to match the VLM model input. |

The element accepts and outputs `video/x-raw,format=RGB`. Output dimensions are negotiated
from `out-width` and `out-height`.

```sh
... ! gvaframeselect frame-budget=8 ! videoconvert ! video/x-raw,format=RGB ! \
	gvaroiscale out-width=448 out-height=448 ! gvagenai ...
```