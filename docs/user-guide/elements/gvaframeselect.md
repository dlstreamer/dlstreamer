# gvaframeselect

Selects up to `frame-budget` evenly spaced frames from each `sub-stream-frames` block.
The selection restarts when the input buffer has `GST_BUFFER_FLAG_DISCONT`; `gvaeventrecorder`
marks the first forwarded frame of each event this way. Input and output video caps are unchanged.

| Property | Type | Range | Default | Description |
|---|---|---|---|---|
| `sub-stream-frames` | Unsigned Integer | [1, 4294967295] | 900 | Length $N$, in input frames, of each selection block. The pattern restarts every $N$ frames and on `DISCONT`. |
| `frame-budget` | Unsigned Integer | [1, 4294967295] | 8 | Maximum number $k$ of frames forwarded per full block, selected evenly across the block. For $k=1$, the first frame is selected. |

For block length $N$ and budget $k$, the selected position is
$\operatorname{round}(i \times (N - 1) / (k - 1))$. If $k=1$, frame index zero is selected;
if $k \geq N$, every frame is forwarded. A short final block forwards only selected positions
that occur before end-of-stream; its selection is not recalculated for the shorter length.

```sh
... ! gvaeventrecorder ! gvaframeselect sub-stream-frames=900 frame-budget=8 ! ...
```