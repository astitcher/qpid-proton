/* Fake stuff necessary to get through parsing */
#define __attribute__(x)
#define __inline
#define __extension__
#define __restrict
typedef int __builtin_va_list;
typedef int __gnuc_va_list;
/* Include used bits of proton headers */
#include "proton/import_export.h"
#include "proton/version.h"
#include "proton/types.h"
#include "proton/object.h"
#include "proton/error.h"
#include "proton/condition.h"
#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"
#include "proton/terminus.h"
#include "proton/delivery.h"
#include "proton/disposition.h"
#include "proton/transport.h"
#include "proton/event.h"
#include "proton/message.h"
#include "proton/sasl.h"
#include "proton/ssl.h"
#include "proton/codec.h"
#include "proton/connection_driver.h"
#include "proton/cid.h"
