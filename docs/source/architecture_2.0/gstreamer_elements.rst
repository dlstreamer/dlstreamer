-----------------------
③ GStreamer Elements
-----------------------

The ``GstDlsTransform`` class serves as shim between C++ interface and GStreamer and used to automatically register any
C++ element as GStreamer element. This shim is responsible for all interactions with GStreamer, GLib type registrations,
and wrapping Intel® DL Streamer C++ objects into GStreamer/GLib objects.

The following class diagram shows how ``GstDlsTransform`` connected to C++ interfaces and GStreamer interfaces.
GStreamer independent blocks colored in blue and GStreamer dependent blocks colored in gray.

.. graphviz::
  :caption: C++ interfaces and classes

  digraph {
    node[shape=record,style=filled,fillcolor=lightskyblue1]
    edge[dir=back, arrowtail=empty]

    gst_plugin_desc[label = "export gst_plugin_desc", fillcolor=gray95]
    GstElement[label = "GstElement", fillcolor=gray95]
    GstBase[label = "GstBaseTransformClass", fillcolor=gray95]
    DlsTransform[label = "GstDlsTransform", fillcolor=gray95]
    Desc[label = "{ElementDesc|+name\l+description\l+author\l+params\l+input_info\l+output_info\l+flags\l+create\l}"]
    Element[label = "{«interface»\nElement|+ init()\l+ get_context(...)\l}"]
    Transform[label = "{«interface»\nTransform|+ set_input_info(...)\l+ set_output_info(...)\l+ get_input_info(...)\l+ get_output_info(...)\l+ process(TensorPtr, TensorPtr)\l+ process(FramePtr, FramePtr)\l}"]
    TransformInplace[label = "{«interface»\nTransformInplace|+ set_info(const FrameInfo&)\l+ process(TensorPtr)\l+ process(FramePtr)\l}"]
    BaseT1[label = "{BaseElement\<Transform\>|+ init_once()\l+ init()\l+ get_context(...)\l}"]
    BaseT2[label = "{BaseElement\<TransformInplace\>|+ init_once()\l+ init()\l+ get_context(...)\l}"]
    BaseTransform[label = "BaseTransform"]
    BaseInplace[label = "BaseTransformInplace"]

    GstElement->GstBase
    GstBase->DlsTransform
    Desc->DlsTransform[arrowtail=vee]
    DlsTransform->gst_plugin_desc
    Element->DlsTransform[constraint=false, arrowtail=vee xlabel="element"]
    Element->{Transform TransformInplace}
    Transform->BaseT1
    TransformInplace->BaseT2
    BaseT1->BaseTransform
    BaseT2->BaseInplace  
  }

Register C++ element as GStreamer element
-----------------------------------------

To register C++ element with GStreamer, ``register_elements_gst_plugin`` function is used.
It should be called from ``plugin_init`` function of GStreamer plugin, here's an example:

.. code:: cpp

  static gboolean plugin_init(GstPlugin *plugin) {
    return register_elements_gst_plugin(tensor_postproc_label, plugin);
  }

How C++ element works as GStreamer element
------------------------------------------

In case of using C++ element in GStreamer pipeline, the shim ``GstDlsTransform`` is responsible for element creation and operation.
The following sequence diagram shows how ``GstDlsTransform`` communicates with GStreamer and an C++ element.
It doesn't show all possible calls, but only the main ones.

.. graphviz::
  :caption: High-level sequence diagram of element creation and operation 

  digraph {
    graph [overlap=true, splines=line, nodesep=1.0, ordering=out];
    edge [arrowhead=none];
    node [shape=none, width=0, height=0, label=""];

    // Head
    {
        rank=same;
        node[shape=rectangle, height=0.5, width=2];
        gst[label="GStreamer"];
        shim[label="GstDlsTransform"];
        cpp[label="C++ element"];
    }
    
    // Vertical lines
    {
        edge [style=dashed, weight=27];
        gst -> a1 -> a2 -> a3 -> a4 -> a5 -> a6 -> a7 -> a8 -> a9 -> a10;
        a10 -> a11 -> a12 -> a13 -> a14 -> a15 -> a16 -> a17 -> a18 -> a19 -> a20 -> a21 -> a22 -> a23 -> a24 -> a25 -> a26;
    }
    {
        edge [style=dashed, weight=27];
        shim -> b1
        b1 -> b2 -> b3 -> b4 -> b5 -> b6 -> b7 -> b8 -> b9 -> b10;
        b10 -> b11 -> b12 -> b13 -> b14 -> b15 -> b16 -> b17 -> b18 -> b19 -> b20 -> b21 -> b22 -> b23 -> b24 -> b25 -> b26;
    }
    {
        edge [style=dashed, weight=27];
        cpp -> c1 -> c2 -> c3 -> c4 -> c5 -> c6 -> c7 -> c8 -> c9 -> c10;
        c10 -> c11 -> c12 -> c13 -> c14 -> c15 -> c16 -> c17 -> c18 -> c19 -> c20 -> c21 -> c22 -> c23 -> c24 -> c25 -> c26;
    }
    
    // Activations
    { rank=same; a1 -> b1 [label="instance_init", arrowhead=normal]; }
    { rank=same; a2 -> b2 [arrowhead=normal, dir=back, style=dashed]; b2 -> c2 [style=invis]; }
    
    { rank=same; a3 -> b3 [arrowhead=normal, label="get/set_property"]; b3 -> c3 [style=invis]; }
    { rank=same; a4 -> b4 [arrowhead=normal, dir=back, style=dashed]; }
    
    { rank=same; a5 -> b5 [arrowhead=normal, label="start"]; b5 -> c5 [style=invis]; }
    { rank=same; a6 -> b6 [style=invis]; b6 -> c6 [arrowhead=normal, label="ElementDesc::create"]; }
    { rank=same; a7 -> b7 [style=invis]; b7 -> c7 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a8 -> b8 [arrowhead=normal, dir=back, style=dashed]; b8 -> c8 [style=invis]; }
    
    { rank=same; a9 -> b9 [arrowhead=normal, label="transform_caps"]; b9 -> c9 [style=invis]; }
    { rank=same; a10 -> b10 [style=invis]; b10 -> c10 [arrowhead=normal, label="set_output_info / set_input_info"]; }
    { rank=same; a11 -> b11 [style=invis]; b11 -> c11 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a12 -> b12 [style=invis]; b12 -> c12 [arrowhead=normal, label="get_input_info / get_output_info"]; }
    { rank=same; a13 -> b13 [style=invis]; b13 -> c13 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a14 -> b14 [arrowhead=normal, dir=back, style=dashed]; b14 -> c14 [style=invis]; }
    
    { rank=same; a15 -> b15 [arrowhead=normal, label="set_caps"]; b15 -> c15 [style=invis]; }
    { rank=same; a16 -> b16 [style=invis]; b16 -> c16 [arrowhead=normal, label="set_input_info"]; }
    { rank=same; a17 -> b17 [style=invis]; b17 -> c17 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a18 -> b18 [style=invis]; b18 -> c18 [arrowhead=normal, label="set_output_info"]; }
    { rank=same; a19 -> b19 [style=invis]; b19 -> c19 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a20 -> b20 [style=invis]; b20 -> c20 [arrowhead=normal, label="init"]; }
    { rank=same; a21 -> b21 [style=invis]; b21 -> c21 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a22 -> b22 [arrowhead=normal, dir=back, style=dashed]; b22 -> c22 [style=invis]; }
    
    { rank=same; a23 -> b23 [arrowhead=normal, label="generate_output"]; b23 -> c23 [style=invis]; }
    { rank=same; a24 -> b24 [style=invis]; b24 -> c24 [arrowhead=normal, label="process"]; }
    { rank=same; a25 -> b25 [style=invis]; b25 -> c25 [arrowhead=normal, dir=back, style=dashed]; }
    { rank=same; a26 -> b26 [arrowhead=normal, dir=back, style=dashed]; b26 -> c26 [style=invis]; }
  }
