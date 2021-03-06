/*
 *  template-history-model.c
 *  Copyright (C) 2009  Jim Evins <evins@snaught.com>.
 *
 *  This file is part of gLabels.
 *
 *  gLabels is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  gLabels is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with gLabels.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "template-history-model.h"

#include <gconf/gconf-client.h>

#include <libglabels.h>
#include "marshal.h"


#define BASE_KEY              "/apps/glabels"
#define RECENT_TEMPLATES_KEY  BASE_KEY "/recent-templates"


/*========================================================*/
/* Private types.                                         */
/*========================================================*/

/** GL_TEMPLATE_HISTORY_MODEL Private fields */
struct _glTemplateHistoryModelPrivate {

	GConfClient *gconf_client;

        guint        max_n;
};

enum {
	CHANGED,
	LAST_SIGNAL
};


/*========================================================*/
/* Private globals.                                       */
/*========================================================*/

static guint signals[LAST_SIGNAL] = {0};


/*========================================================*/
/* Private function prototypes.                           */
/*========================================================*/

static void gl_template_history_model_finalize (GObject                 *object);

static void conf_notify_cb                     (GConfClient             *client,
                                                guint                    cnxn_id,
                                                GConfEntry              *entry,
                                                glTemplateHistoryModel  *this);


/*****************************************************************************/
/* Object infrastructure.                                                    */
/*****************************************************************************/
G_DEFINE_TYPE (glTemplateHistoryModel, gl_template_history_model, G_TYPE_OBJECT);


/*****************************************************************************/
/* Class Init Function.                                                      */
/*****************************************************************************/
static void
gl_template_history_model_class_init (glTemplateHistoryModelClass *class)
{
        GObjectClass  *gobject_class = (GObjectClass *) class;

        gl_template_history_model_parent_class = g_type_class_peek_parent (class);

        gobject_class->finalize = gl_template_history_model_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (glTemplateHistoryModelClass, changed),
			      NULL, NULL,
			      gl_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}


/*****************************************************************************/
/* Object Instance Init Function.                                            */
/*****************************************************************************/
static void
gl_template_history_model_init (glTemplateHistoryModel *this)
{
        this->priv = g_new0 (glTemplateHistoryModelPrivate, 1);

        this->priv->gconf_client = gconf_client_get_default ();

        g_return_if_fail (this->priv->gconf_client != NULL);

        gconf_client_add_dir (this->priv->gconf_client,
                              BASE_KEY,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);

        gconf_client_notify_add (this->priv->gconf_client,
                                 RECENT_TEMPLATES_KEY,
                                 (GConfClientNotifyFunc)conf_notify_cb, this,
                                 NULL, NULL);
}


/*****************************************************************************/
/* Finalize Method.                                                          */
/*****************************************************************************/
static void
gl_template_history_model_finalize (GObject *object)
{
        glTemplateHistoryModel    *this;

        g_return_if_fail (object && IS_GL_TEMPLATE_HISTORY_MODEL (object));
        this = GL_TEMPLATE_HISTORY_MODEL (object);

        g_object_unref (G_OBJECT(this->priv->gconf_client));
        g_free (this->priv);

        G_OBJECT_CLASS (gl_template_history_model_parent_class)->finalize (object);
}


/*****************************************************************************/
/** New Object Generator.                                                    */
/*****************************************************************************/
glTemplateHistoryModel *
gl_template_history_model_new (guint n)
{
        glTemplateHistoryModel *this;

        this = g_object_new (TYPE_GL_TEMPLATE_HISTORY_MODEL, NULL);

        this->priv->max_n = n;

        return this;
}


/*****************************************************************************/
/* Add template to history.                                                      */
/*****************************************************************************/
void
gl_template_history_model_add_name (glTemplateHistoryModel *this,
                                    const gchar            *name)
{
        GSList *list = NULL;
        GList  *old_list;
        GList  *p;
        GSList *ps;

        /*
         * Start new list with this name.
         */
        list = g_slist_append (list, (gchar *)name);

        /*
         * Transfer old list to new list, ignoring any duplicate of this name
         */
        old_list = gl_template_history_model_get_name_list (this);
        for ( p = old_list; p; p=p->next )
        {
                if ( lgl_str_utf8_casecmp (name, p->data) )
                {
                        list = g_slist_append (list, p->data);
                }
                else
                {
                        g_free (p->data);
                }
        }
        g_list_free (old_list);

        /*
         * Truncate list to maximum size
         */
        while (g_slist_length (list) > this->priv->max_n)
        {
                ps = g_slist_last (list);
                list = g_slist_remove_link (list, ps);
                g_slist_free_1 (ps);
        }

        /*
         * Update conf
         */
        gconf_client_set_list (this->priv->gconf_client,
                               RECENT_TEMPLATES_KEY,
                               GCONF_VALUE_STRING,
                               list,
                               NULL);
}


/*****************************************************************************/
/* GConf notify callback.                                                    */
/*****************************************************************************/
static void
conf_notify_cb (GConfClient         *client,
                guint                cnxn_id,
                GConfEntry          *entry,
                glTemplateHistoryModel  *this)
{
        g_signal_emit (G_OBJECT(this), signals[CHANGED], 0);
}


/*****************************************************************************/
/* Get list of template families.                                            */
/*****************************************************************************/
GList *
gl_template_history_model_get_name_list (glTemplateHistoryModel *this)
{
        GList  *list = NULL;
        GSList *tmp_list;
        GSList *p;

        /*
         * Get name list.
         */
	tmp_list = gconf_client_get_list (this->priv->gconf_client,
                                          RECENT_TEMPLATES_KEY,
                                          GCONF_VALUE_STRING,
                                          NULL);

        /*
         * Proof read name list; transfer storage to new list.
         */
        for (p=tmp_list; p != NULL; p=p->next)
        {
                if ( lgl_db_does_template_name_exist (p->data) )
                {
                        list = g_list_append (list, p->data);
                }
                else
                {
                        g_free (p->data);
                }
        }
        g_slist_free (tmp_list);

        return list;
}


/*****************************************************************************/
/* Free template name list.                                                  */
/*****************************************************************************/
void
gl_template_history_model_free_name_list (GList *list)
{
        GList *p;

        for ( p = list; p; p=p->next )
        {
                g_free (p->data);
        }
        g_list_free (list);
}



/*
 * Local Variables:       -- emacs
 * mode: C                -- emacs
 * c-basic-offset: 8      -- emacs
 * tab-width: 8           -- emacs
 * indent-tabs-mode: nil  -- emacs
 * End:                   -- emacs
 */
