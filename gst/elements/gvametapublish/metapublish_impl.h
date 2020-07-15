/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __METAPUBLISHIMPL_H__
#define __METAPUBLISHIMPL_H__

#include "metapublish_impl_types.h"

MetapublishStatusMessage OpenConnection(GstGvaMetaPublish *);
MetapublishStatusMessage CloseConnection(GstGvaMetaPublish *);
MetapublishStatusMessage WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf);

#endif /* __METAPUBLISHIMPL_H__ */
