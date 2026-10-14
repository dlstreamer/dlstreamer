# gvaroiscale

Finds the first valid object-detection ROI in `GstAnalyticsRelationMeta`, crops it from an
RGB frame, preserves aspect ratio, and centers the resized image on a black canvas. Frames
without a valid ROI are dropped.

| Property | Type | Range | Default | Description |
|---|---|---|---|---|
| `target-classes` | String | — | (empty) | Comma-separated detection labels to consider; an empty value allows all classes. |
| `out-width` | Unsigned Integer | [1, 8192] | 448 | Output width; configure for the model input expected by the next element. |
| `out-height` | Unsigned Integer | [1, 8192] | 448 | Output height; configure for the model input expected by the next element. |
| `output-dir` | String | — | (empty) | If set, each scaled ROI frame is also saved as `frame_XXXXXXXX.jpg` in this directory. The directory is created if needed. |

The element accepts and outputs `video/x-raw,format=RGB`. Output dimensions are negotiated
from `out-width` and `out-height`.
Set these dimensions according to the downstream model's input requirements. Models that accept
dynamic image sizes or resize inputs internally may not require an exact match; check the model
documentation. The defaults are not universal.
The output directory is optional; an empty value disables image saving. File numbering starts
at zero and restarts on each new stream.
When `target-classes` is set, only ROIs whose labels match one of the comma-separated values are
considered. Whitespace around labels is ignored.

```sh
... ! gvaframeselect frame-budget=8 ! videoconvert ! video/x-raw,format=RGB ! \
	gvaroiscale out-width=448 out-height=448 target-classes=person,vehicle ! gvagenai ...
```