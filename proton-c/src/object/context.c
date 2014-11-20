/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <proton/object.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
  pn_handle_t key;
  const pn_class_t *clazz;
  void *value;
} pni_entry_t;

struct pn_context_t {
  size_t size;
  pni_entry_t *entries;
};

static void pn_context_initialize(void *object)
{
  pn_context_t *ctx = (pn_context_t *) object;
  ctx->size = 0;
  ctx->entries = NULL;
}

static void pn_context_finalize(void *object)
{
  pn_context_t *ctx = (pn_context_t *) object;
  for (size_t i = 0; i < ctx->size; i++) {
    pni_entry_t *v = &ctx->entries[i];
    pn_class_decref(v->clazz, v->value);
  }
  free(ctx->entries);
}

#define pn_context_hashcode NULL
#define pn_context_compare NULL
#define pn_context_inspect NULL

pn_context_t *pn_context(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_context);
  pn_context_t *ctx = (pn_context_t *) pn_class_new(&clazz, sizeof(pn_context_t));
  pn_context_def(ctx, PN_LEGCTX, PN_VOID);
  return ctx;
}

static pni_entry_t *pni_context_find(pn_context_t *ctx, pn_handle_t key) {
  for (size_t i = 0; i < ctx->size; i++) {
    pni_entry_t *entry = &ctx->entries[i];
    if (entry->key == key) {
      return entry;
    }
  }
  return NULL;
}

static pni_entry_t *pni_context_create(pn_context_t *ctx) {
  ctx->size++;
  ctx->entries = (pni_entry_t *) realloc(ctx->entries, ctx->size * sizeof(pni_entry_t));
  pni_entry_t *entry = &ctx->entries[ctx->size - 1];
  entry->key = 0;
  entry->clazz = NULL;
  entry->value = NULL;
  return entry;
}

void pn_context_def(pn_context_t *context, pn_handle_t key, const pn_class_t *clazz)
{
  assert(context);
  assert(clazz);

  pni_entry_t *entry = pni_context_find(context, key);
  if (entry) {
    assert(entry->clazz == clazz);
  } else {
    entry = pni_context_create(context);
    entry->key = key;
    entry->clazz = clazz;
  }
}

void *pn_context_get(pn_context_t *context, pn_handle_t key)
{
  assert(context);
  pni_entry_t *entry = pni_context_find(context, key);
  if (entry) {
    return entry->value;
  } else {
    return NULL;
  }
}

void pn_context_set(pn_context_t *context, pn_handle_t key, void *value)
{
  assert(context);

  pni_entry_t *entry = pni_context_find(context, key);
  if (entry) {
    void *old = entry->value;
    entry->value = value;
    pn_class_incref(entry->clazz, value);
    pn_class_decref(entry->clazz, old);
  }
}
