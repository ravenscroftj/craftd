/*
 * Copyright (c) 2010-2011 Kevin M. Bowling, <kevin.bowling@kev009.com>, USA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Worker.h"
#include "common.h"

CDWorker*
CD_CreateWorker (void)
{
    CDWorker* self = CD_malloc(sizeof(CDWorker));

    if (!object) {
        return NULL;
    }

    self->id      = 0;
    self->working = NULL;
    self->thread  = NULL;

    return object;
}

void
CD_DestroyWorker (CDWorker* self)
{
    if (self->thread) {
        self->working = false;
        pthread_cancel(self->thread);
    }

    CD_free(self->working);
    CD_free(self);
}

void*
CD_RunWorker (void* arg)
{
    CDWorker* self = arg;
    CDPacket* packet;

    LOG(LOG_INFO, "Worker %d started!", id);

    while (self->working) {
        pthread_mutex_lock(&self->workers->mutex);

        /* Check our predicate again:  The WQ is not empty
         * Prevent a nasty race condition if the client disconnects
         * Works in tandem with errorcb FOREACH bev removal loop
         */
        while (CD_ListLength(self->workers->jobs) == 0) {
            LOGT(LOG_DEBUG, "Worker %d ready", self->id);

            pthread_cond_wait(&self->workers->condition, &self->workers->mutex);
        }

        LOGT(LOG_DEBUG, "in worker: %d", self->id);

        self->job = CD_ListShift(self->workers->jobs);
        pthread_mutex_unlock(self->workers->mutex);

        if (self->job->lock != NULL) { //this will auto lock any type of WQ
            pthread_rwlock_wrlock(self->job->lock);
        }

        if (self->job->type == CDGameInput || self->job->type == CDProxyInput) {
            if (!self->job->event || !self->job->player) {
                LOG(LOG_CRIT, "Aaack, null event or context in worker?");

                goto WORKER_ERR;
            }

            CD_SetPrivateData(self->player, "packet", CD_PacketFromEvent(self->workers->server, self->job->event));
        }
    }

    return NULL;
}