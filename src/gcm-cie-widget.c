/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
 *
 * Color conversion algorithms taken from ppmcie:
 *   Copyright (C) 1999 John Walker <kelvin@fourmilab.ch>
 *   Copyright (C) 1999 Andrew J. S. Hamilton <Andrew.Hamilton@Colorado.EDU>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>

#include "gcm-cie-widget.h"

G_DEFINE_TYPE (GcmCieWidget, gcm_cie_widget, GTK_TYPE_DRAWING_AREA);
#define GCM_CIE_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CIE_WIDGET, GcmCieWidgetPrivate))
#define GCM_CIE_WIDGET_FONT "Sans 8"

struct GcmCieWidgetPrivate
{
	gboolean		 use_grid;
	gboolean		 use_whitepoint;
	guint			 chart_width;
	guint			 chart_height;
	PangoLayout		*layout;
	GPtrArray		*tongue_buffer;			/* min and max of the tongue shape */
	guint			 x_offset;
	guint			 y_offset;

	/* CIE x and y coordinates of its three primary illuminants and the
	 * x and y coordinates of the white point. */
	CdColorYxy		*red;			/* red primary illuminant */
	CdColorYxy		*green;			/* green primary illuminant */
	CdColorYxy		*blue;			/* blue primary illuminant */
	CdColorYxy		*white;			/* white point */
	gdouble			 gamma;			/* gamma of nonlinear correction */
};

/* The following table gives the spectral chromaticity co-ordinates
 * for wavelengths in one nanometre increments from 380nm through 780nm */
static const gdouble spectral_chromaticity[][3] = {
	{ 0.1741, 0.0050 },	/* 380 nm */
	{ 0.1741, 0.0050 },
	{ 0.1741, 0.0050 },
	{ 0.1741, 0.0050 },
	{ 0.1740, 0.0050 },
	{ 0.1740, 0.0050 },
	{ 0.1740, 0.0050 },
	{ 0.1739, 0.0049 },
	{ 0.1739, 0.0049 },
	{ 0.1738, 0.0049 },
	{ 0.1738, 0.0049 },
	{ 0.1738, 0.0049 },
	{ 0.1737, 0.0049 },
	{ 0.1737, 0.0049 },
	{ 0.1736, 0.0049 },
	{ 0.1736, 0.0049 },
	{ 0.1735, 0.0049 },
	{ 0.1735, 0.0049 },
	{ 0.1734, 0.0048 },
	{ 0.1734, 0.0048 },
	{ 0.1733, 0.0048 },
	{ 0.1733, 0.0048 },
	{ 0.1732, 0.0048 },
	{ 0.1732, 0.0048 },
	{ 0.1731, 0.0048 },
	{ 0.1730, 0.0048 },
	{ 0.1729, 0.0048 },
	{ 0.1728, 0.0048 },
	{ 0.1728, 0.0048 },
	{ 0.1727, 0.0048 },
	{ 0.1726, 0.0048 },
	{ 0.1725, 0.0048 },
	{ 0.1724, 0.0048 },
	{ 0.1723, 0.0048 },
	{ 0.1722, 0.0048 },
	{ 0.1721, 0.0048 },
	{ 0.1720, 0.0049 },
	{ 0.1719, 0.0049 },
	{ 0.1717, 0.0049 },
	{ 0.1716, 0.0050 },
	{ 0.1714, 0.0051 },
	{ 0.1712, 0.0052 },
	{ 0.1710, 0.0053 },
	{ 0.1708, 0.0055 },
	{ 0.1705, 0.0056 },
	{ 0.1703, 0.0058 },
	{ 0.1701, 0.0060 },
	{ 0.1698, 0.0062 },
	{ 0.1695, 0.0064 },
	{ 0.1692, 0.0066 },
	{ 0.1689, 0.0069 },
	{ 0.1685, 0.0072 },
	{ 0.1681, 0.0075 },
	{ 0.1677, 0.0078 },
	{ 0.1673, 0.0082 },
	{ 0.1669, 0.0086 },
	{ 0.1664, 0.0090 },
	{ 0.1660, 0.0094 },
	{ 0.1655, 0.0099 },
	{ 0.1650, 0.0104 },
	{ 0.1644, 0.0109 },
	{ 0.1638, 0.0114 },
	{ 0.1632, 0.0119 },
	{ 0.1626, 0.0125 },
	{ 0.1619, 0.0131 },
	{ 0.1611, 0.0138 },
	{ 0.1603, 0.0145 },
	{ 0.1595, 0.0152 },
	{ 0.1586, 0.0160 },
	{ 0.1576, 0.0168 },
	{ 0.1566, 0.0177 },
	{ 0.1556, 0.0186 },
	{ 0.1545, 0.0196 },
	{ 0.1534, 0.0206 },
	{ 0.1522, 0.0216 },
	{ 0.1510, 0.0227 },
	{ 0.1497, 0.0240 },
	{ 0.1483, 0.0252 },
	{ 0.1469, 0.0266 },
	{ 0.1455, 0.0281 },
	{ 0.1440, 0.0297 },
	{ 0.1424, 0.0314 },
	{ 0.1408, 0.0332 },
	{ 0.1391, 0.0352 },
	{ 0.1374, 0.0374 },
	{ 0.1355, 0.0399 },
	{ 0.1335, 0.0427 },
	{ 0.1314, 0.0459 },
	{ 0.1291, 0.0494 },
	{ 0.1267, 0.0534 },
	{ 0.1241, 0.0578 },
	{ 0.1215, 0.0626 },
	{ 0.1187, 0.0678 },
	{ 0.1158, 0.0736 },
	{ 0.1128, 0.0799 },
	{ 0.1096, 0.0868 },
	{ 0.1063, 0.0945 },
	{ 0.1028, 0.1029 },
	{ 0.0991, 0.1120 },
	{ 0.0953, 0.1219 },
	{ 0.0913, 0.1327 },
	{ 0.0871, 0.1443 },
	{ 0.0827, 0.1569 },
	{ 0.0781, 0.1704 },
	{ 0.0734, 0.1850 },
	{ 0.0687, 0.2007 },
	{ 0.0640, 0.2175 },
	{ 0.0593, 0.2353 },
	{ 0.0547, 0.2541 },
	{ 0.0500, 0.2740 },
	{ 0.0454, 0.2950 },
	{ 0.0408, 0.3170 },
	{ 0.0362, 0.3399 },
	{ 0.0318, 0.3636 },
	{ 0.0275, 0.3879 },
	{ 0.0235, 0.4127 },
	{ 0.0197, 0.4378 },
	{ 0.0163, 0.4630 },
	{ 0.0132, 0.4882 },
	{ 0.0105, 0.5134 },
	{ 0.0082, 0.5384 },
	{ 0.0063, 0.5631 },
	{ 0.0049, 0.5871 },
	{ 0.0040, 0.6104 },
	{ 0.0036, 0.6330 },
	{ 0.0039, 0.6548 },
	{ 0.0046, 0.6759 },
	{ 0.0060, 0.6961 },
	{ 0.0080, 0.7153 },
	{ 0.0106, 0.7334 },
	{ 0.0139, 0.7502 },
	{ 0.0178, 0.7656 },
	{ 0.0222, 0.7796 },
	{ 0.0273, 0.7921 },
	{ 0.0328, 0.8029 },
	{ 0.0389, 0.8120 },
	{ 0.0453, 0.8194 },
	{ 0.0522, 0.8252 },
	{ 0.0593, 0.8294 },
	{ 0.0667, 0.8323 },
	{ 0.0743, 0.8338 },
	{ 0.0821, 0.8341 },
	{ 0.0899, 0.8333 },
	{ 0.0979, 0.8316 },
	{ 0.1060, 0.8292 },
	{ 0.1142, 0.8262 },
	{ 0.1223, 0.8228 },
	{ 0.1305, 0.8189 },
	{ 0.1387, 0.8148 },
	{ 0.1468, 0.8104 },
	{ 0.1547, 0.8059 },
	{ 0.1625, 0.8012 },
	{ 0.1702, 0.7965 },
	{ 0.1778, 0.7917 },
	{ 0.1854, 0.7867 },
	{ 0.1929, 0.7816 },
	{ 0.2003, 0.7764 },
	{ 0.2077, 0.7711 },
	{ 0.2150, 0.7656 },
	{ 0.2223, 0.7600 },
	{ 0.2296, 0.7543 },
	{ 0.2369, 0.7485 },
	{ 0.2441, 0.7426 },
	{ 0.2514, 0.7366 },
	{ 0.2586, 0.7305 },
	{ 0.2658, 0.7243 },
	{ 0.2730, 0.7181 },
	{ 0.2801, 0.7117 },
	{ 0.2873, 0.7053 },
	{ 0.2945, 0.6988 },
	{ 0.3016, 0.6923 },
	{ 0.3088, 0.6857 },
	{ 0.3159, 0.6791 },
	{ 0.3231, 0.6724 },
	{ 0.3302, 0.6656 },
	{ 0.3374, 0.6588 },
	{ 0.3445, 0.6520 },
	{ 0.3517, 0.6452 },
	{ 0.3588, 0.6383 },
	{ 0.3660, 0.6314 },
	{ 0.3731, 0.6245 },
	{ 0.3802, 0.6175 },
	{ 0.3874, 0.6105 },
	{ 0.3945, 0.6036 },
	{ 0.4016, 0.5966 },
	{ 0.4087, 0.5896 },
	{ 0.4158, 0.5826 },
	{ 0.4229, 0.5756 },
	{ 0.4300, 0.5686 },
	{ 0.4370, 0.5617 },
	{ 0.4441, 0.5547 },
	{ 0.4511, 0.5478 },
	{ 0.4580, 0.5408 },
	{ 0.4650, 0.5339 },
	{ 0.4719, 0.5271 },
	{ 0.4788, 0.5202 },
	{ 0.4856, 0.5134 },
	{ 0.4924, 0.5066 },
	{ 0.4992, 0.4999 },
	{ 0.5058, 0.4932 },
	{ 0.5125, 0.4866 },
	{ 0.5191, 0.4800 },
	{ 0.5256, 0.4735 },
	{ 0.5321, 0.4671 },
	{ 0.5385, 0.4607 },
	{ 0.5448, 0.4544 },
	{ 0.5510, 0.4482 },
	{ 0.5572, 0.4421 },
	{ 0.5633, 0.4361 },
	{ 0.5693, 0.4301 },
	{ 0.5752, 0.4242 },
	{ 0.5810, 0.4184 },
	{ 0.5867, 0.4128 },
	{ 0.5922, 0.4072 },
	{ 0.5977, 0.4018 },
	{ 0.6029, 0.3965 },
	{ 0.6080, 0.3914 },
	{ 0.6130, 0.3865 },
	{ 0.6178, 0.3817 },
	{ 0.6225, 0.3770 },
	{ 0.6270, 0.3725 },
	{ 0.6315, 0.3680 },
	{ 0.6359, 0.3637 },
	{ 0.6402, 0.3594 },
	{ 0.6443, 0.3553 },
	{ 0.6482, 0.3514 },
	{ 0.6520, 0.3476 },
	{ 0.6557, 0.3440 },
	{ 0.6592, 0.3406 },
	{ 0.6625, 0.3372 },
	{ 0.6658, 0.3340 },
	{ 0.6689, 0.3309 },
	{ 0.6719, 0.3279 },
	{ 0.6747, 0.3251 },
	{ 0.6775, 0.3224 },
	{ 0.6801, 0.3197 },
	{ 0.6826, 0.3172 },
	{ 0.6850, 0.3149 },
	{ 0.6873, 0.3126 },
	{ 0.6894, 0.3104 },
	{ 0.6915, 0.3083 },
	{ 0.6935, 0.3064 },
	{ 0.6954, 0.3045 },
	{ 0.6972, 0.3027 },
	{ 0.6989, 0.3010 },
	{ 0.7006, 0.2993 },
	{ 0.7022, 0.2977 },
	{ 0.7037, 0.2962 },
	{ 0.7052, 0.2948 },
	{ 0.7066, 0.2934 },
	{ 0.7079, 0.2920 },
	{ 0.7092, 0.2907 },
	{ 0.7105, 0.2895 },
	{ 0.7117, 0.2882 },
	{ 0.7129, 0.2871 },
	{ 0.7140, 0.2859 },
	{ 0.7151, 0.2848 },
	{ 0.7162, 0.2838 },
	{ 0.7172, 0.2828 },
	{ 0.7181, 0.2819 },
	{ 0.7190, 0.2809 },
	{ 0.7199, 0.2801 },
	{ 0.7208, 0.2792 },
	{ 0.7216, 0.2784 },
	{ 0.7223, 0.2777 },
	{ 0.7230, 0.2769 },
	{ 0.7237, 0.2763 },
	{ 0.7243, 0.2757 },
	{ 0.7249, 0.2751 },
	{ 0.7255, 0.2745 },
	{ 0.7260, 0.2740 },
	{ 0.7265, 0.2735 },
	{ 0.7270, 0.2730 },
	{ 0.7274, 0.2726 },
	{ 0.7279, 0.2721 },
	{ 0.7283, 0.2717 },
	{ 0.7287, 0.2713 },
	{ 0.7290, 0.2710 },
	{ 0.7294, 0.2706 },
	{ 0.7297, 0.2703 },
	{ 0.7300, 0.2700 },
	{ 0.7302, 0.2698 },
	{ 0.7305, 0.2695 },
	{ 0.7307, 0.2693 },
	{ 0.7309, 0.2691 },
	{ 0.7311, 0.2689 },
	{ 0.7313, 0.2687 },
	{ 0.7315, 0.2685 },
	{ 0.7316, 0.2684 },
	{ 0.7318, 0.2682 },
	{ 0.7320, 0.2680 },
	{ 0.7322, 0.2678 },
	{ 0.7323, 0.2677 },
	{ 0.7324, 0.2676 },
	{ 0.7326, 0.2674 },
	{ 0.7327, 0.2673 },
	{ 0.7329, 0.2671 },
	{ 0.7330, 0.2670 },
	{ 0.7331, 0.2669 },
	{ 0.7333, 0.2667 },
	{ 0.7334, 0.2666 },
	{ 0.7336, 0.2664 },
	{ 0.7337, 0.2663 },
	{ 0.7338, 0.2662 },
	{ 0.7339, 0.2661 },
	{ 0.7340, 0.2660 },
	{ 0.7341, 0.2659 },
	{ 0.7342, 0.2658 },
	{ 0.7343, 0.2657 },
	{ 0.7343, 0.2657 },
	{ 0.7344, 0.2656 },
	{ 0.7344, 0.2656 },
	{ 0.7345, 0.2655 },
	{ 0.7345, 0.2655 },
	{ 0.7346, 0.2654 },
	{ 0.7346, 0.2654 },
	{ 0.7346, 0.2654 },
	{ 0.7346, 0.2654 },
	{ 0.7347, 0.2653 },
	{ 0.7347, 0.2653 },
	{ 0.7347, 0.2653 }	/* 780 nm */
};

typedef struct {
	guint		 min;
	guint		 max;
	gboolean	 valid;
} GcmCieWidgetBufferItem;

static gboolean gcm_cie_widget_draw (GtkWidget *cie, cairo_t *cr);
static void	gcm_cie_widget_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_USE_GRID,
	PROP_USE_WHITEPOINT,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_WHITE,
	PROP_LAST
};

static void
gcm_cie_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmCieWidget *cie = GCM_CIE_WIDGET (object);
	switch (prop_id) {
	case PROP_USE_GRID:
		g_value_set_boolean (value, cie->priv->use_grid);
		break;
	case PROP_USE_WHITEPOINT:
		g_value_set_boolean (value, cie->priv->use_whitepoint);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gcm_cie_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmCieWidget *cie = GCM_CIE_WIDGET (object);
	GcmCieWidgetPrivate *priv = cie->priv;

	switch (prop_id) {
	case PROP_USE_GRID:
		cie->priv->use_grid = g_value_get_boolean (value);
		break;
	case PROP_USE_WHITEPOINT:
		cie->priv->use_whitepoint = g_value_get_boolean (value);
		break;
	case PROP_RED:
		cd_color_yxy_copy (g_value_get_boxed (value), priv->red);
		break;
	case PROP_GREEN:
		cd_color_yxy_copy (g_value_get_boxed (value), priv->green);
		break;
	case PROP_BLUE:
		cd_color_yxy_copy (g_value_get_boxed (value), priv->blue);
		break;
	case PROP_WHITE:
		cd_color_yxy_copy (g_value_get_boxed (value), priv->white);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* refresh widget */
	gtk_widget_hide (GTK_WIDGET (cie));
	gtk_widget_show (GTK_WIDGET (cie));
}

static void
gcm_cie_widget_class_init (GcmCieWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->draw = gcm_cie_widget_draw;
	object_class->get_property = gcm_cie_get_property;
	object_class->set_property = gcm_cie_set_property;
	object_class->finalize = gcm_cie_widget_finalize;

	g_type_class_add_private (class, sizeof (GcmCieWidgetPrivate));

	/* properties */
	g_object_class_install_property (object_class,
					 PROP_USE_GRID,
					 g_param_spec_boolean ("use-grid", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_USE_WHITEPOINT,
					 g_param_spec_boolean ("use-whitepoint", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_RED,
					 g_param_spec_boxed ("red", NULL, NULL,
							     CD_TYPE_COLOR_YXY,
							     G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					 PROP_GREEN,
					 g_param_spec_boxed ("green", NULL, NULL,
							     CD_TYPE_COLOR_YXY,
							     G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					 PROP_BLUE,
					 g_param_spec_boxed ("blue", NULL, NULL,
							     CD_TYPE_COLOR_YXY,
							     G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					 PROP_WHITE,
					 g_param_spec_boxed ("white", NULL, NULL,
							     CD_TYPE_COLOR_YXY,
							     G_PARAM_WRITABLE));
}

void
gcm_cie_widget_set_from_profile (GtkWidget *widget, CdIcc *profile)
{
	GcmCieWidget *cie = GCM_CIE_WIDGET (widget);
	CdColorXYZ *white;
	CdColorXYZ *red;
	CdColorXYZ *green;
	CdColorXYZ *blue;

	/* get the new details from the profile */
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);

	/* copy into this widget */
	cd_color_xyz_to_yxy (white, cie->priv->white);
	cd_color_xyz_to_yxy (red, cie->priv->red);
	cd_color_xyz_to_yxy (green, cie->priv->green);
	cd_color_xyz_to_yxy (blue, cie->priv->blue);

	/* hide if we have no data */
	if (cie->priv->white->x > 0.001) {
		gtk_widget_hide (widget);
		gtk_widget_show (widget);
	} else {
		gtk_widget_hide (widget);
	}

	/* free */
	cd_color_xyz_free (white);
	cd_color_xyz_free (red);
	cd_color_xyz_free (green);
	cd_color_xyz_free (blue);
}

static void
gcm_cie_widget_init (GcmCieWidget *cie)
{
	PangoContext *context;
	PangoFontDescription *desc;

	cie->priv = GCM_CIE_WIDGET_GET_PRIVATE (cie);
	cie->priv->use_grid = TRUE;
	cie->priv->use_whitepoint = TRUE;
	cie->priv->tongue_buffer = g_ptr_array_new_with_free_func (g_free);

	/* default is CIE REC 709 */
	cie->priv->red = cd_color_yxy_new ();
	cie->priv->green = cd_color_yxy_new ();
	cie->priv->blue = cd_color_yxy_new ();
	cie->priv->white = cd_color_yxy_new ();
	cie->priv->red->x = 0.64;
	cie->priv->red->y = 0.33;
	cie->priv->green->x = 0.30;
	cie->priv->green->y = 0.60;
	cie->priv->blue->x = 0.15;
	cie->priv->blue->y = 0.06;
	cie->priv->white->x = 0.3127;
	cie->priv->white->y = 0.3291;
	cie->priv->gamma = 0.0;

	/* do pango stuff */
	context =  gtk_widget_get_pango_context (GTK_WIDGET (cie));
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);

	cie->priv->layout = pango_layout_new (context);
	desc = pango_font_description_from_string (GCM_CIE_WIDGET_FONT);
	pango_layout_set_font_description (cie->priv->layout, desc);
	pango_font_description_free (desc);
}

static void
gcm_cie_widget_finalize (GObject *object)
{
	GcmCieWidget *cie = (GcmCieWidget*) object;

	g_object_unref (cie->priv->layout);
	cd_color_yxy_free (cie->priv->white);
	cd_color_yxy_free (cie->priv->red);
	cd_color_yxy_free (cie->priv->green);
	cd_color_yxy_free (cie->priv->blue);
	g_ptr_array_unref (cie->priv->tongue_buffer);
	G_OBJECT_CLASS (gcm_cie_widget_parent_class)->finalize (object);
}

static void
gcm_cie_widget_draw_grid (GcmCieWidget *cie, cairo_t *cr)
{
	guint i;
	gdouble b;
	gdouble dotted[] = {1., 2.};
	gdouble divwidth  = (gdouble)cie->priv->chart_width / 10.0f;
	gdouble divheight = (gdouble)cie->priv->chart_height / 10.0f;

	cairo_save (cr);
	cairo_set_line_width (cr, 1);
	cairo_set_dash (cr, dotted, 2, 0.0);

	/* do vertical lines */
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	for (i=1; i<10; i++) {
		b = ((gdouble) i * divwidth);
		cairo_move_to (cr, (gint)b + 0.5f, 0);
		cairo_line_to (cr, (gint)b + 0.5f, cie->priv->chart_height);
		cairo_stroke (cr);
	}

	/* do horizontal lines */
	for (i=1; i<10; i++) {
		b = ((gdouble) i * divheight);
		cairo_move_to (cr, 0, (gint)b + 0.5f);
		cairo_line_to (cr, cie->priv->chart_width, (int)b + 0.5f);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static void
gcm_cie_widget_map_to_display (GcmCieWidget *cie, gdouble x, gdouble y, gdouble *x_retval, gdouble *y_retval)
{
	GcmCieWidgetPrivate *priv = cie->priv;

	*x_retval = (x * (priv->chart_width - 1)) + priv->x_offset;
	*y_retval = ((priv->chart_height - 1) - y * (priv->chart_height - 1)) - priv->y_offset;
}

static void
gcm_cie_widget_map_from_display (GcmCieWidget *cie, gdouble x, gdouble y, gdouble *x_retval, gdouble *y_retval)
{
	GcmCieWidgetPrivate *priv = cie->priv;

	*x_retval = ((gdouble) x - priv->x_offset) / (priv->chart_width - 1);
	*y_retval = 1.0 - ((gdouble) y + priv->y_offset) / (priv->chart_height - 1);
}

static void
gcm_cie_widget_compute_monochrome_color_location (GcmCieWidget *cie, gdouble wave_length,
						  gdouble *x_retval, gdouble *y_retval)
{
	guint ix = wave_length - 380;
	const gdouble px = spectral_chromaticity[ix][0];
	const gdouble py = spectral_chromaticity[ix][1];

	/* convert to screen co-ordinates */
	gcm_cie_widget_map_to_display (cie, px, py, x_retval, y_retval);
}

static void
gcm_cie_widget_save_point (GcmCieWidget *cie, const guint y, const gdouble value)
{
	GcmCieWidgetBufferItem *item;
	GcmCieWidgetPrivate *priv = cie->priv;

	if (y >= priv->tongue_buffer->len)
		return;
	item = g_ptr_array_index (priv->tongue_buffer, y);
	if (item->valid) {
		if (value < item->min)
			item->min = value;
		if (value > item->max)
			item->max = value;
	} else {
		item->min = value;
		item->valid = TRUE;
	}
}

static void
gcm_cie_widget_add_point (GcmCieWidget *cie, gdouble icx, gdouble icy, gdouble icx_last, gdouble icy_last)
{
	gdouble grad;
	gdouble i;
	gdouble dy, dx;
	gdouble c;
	gdouble x;

	/* nothing to plot */
	if (icx > cie->priv->chart_width)
		return;
	if (icy > cie->priv->chart_height)
		return;

	/* nothing to plot */
	if (icx == icx_last && icy == icy_last)
		return;

	/* no change in y axis */
	if (icy == icy_last)
		return;

	/* trivial */
	if (icx == icx_last) {
		for (i=icy; i<=icy_last; i++)
			gcm_cie_widget_save_point (cie, i, icx);
		for (i=icy_last; i<=icy; i++)
			gcm_cie_widget_save_point (cie, i, icx);
		return;
	}

	/* plot on the buffer */
	dy = icy - icy_last;
	dx = icx - icx_last;
	grad = ((gdouble) dy) / ((gdouble) dx);
	c = icy - (grad * (gdouble) icx);
	for (i=icy; i<=icy_last; i++) {
		x = (i - c) / grad;
		gcm_cie_widget_save_point (cie, i, x);
	}
	for (i=icy_last; i<=icy; i++) {
		x = (i - c) / grad;
		gcm_cie_widget_save_point (cie, i, x);
	}
}

static void
gcm_cie_widget_get_min_max_tongue (GcmCieWidget *cie)
{
	guint wavelength;
	gdouble icx, icy;
	gdouble icx_last, icy_last;
	GcmCieWidgetBufferItem *item;
	guint i;
	GcmCieWidgetPrivate *priv = cie->priv;

	/* add enough elements to the array */
	g_ptr_array_set_size (priv->tongue_buffer, 0);
	for (i = 0; i < priv->chart_height; i++) {
		item = g_new0 (GcmCieWidgetBufferItem, 1);
		g_ptr_array_add (priv->tongue_buffer, item);
	}

	/* get first co-ordinate */
	gcm_cie_widget_compute_monochrome_color_location (cie, 380, &icx_last, &icy_last);

	/* this is fast path */
	for (wavelength = 380+1; wavelength <= 700; wavelength++) {
		gcm_cie_widget_compute_monochrome_color_location (cie, wavelength, &icx, &icy);
		gcm_cie_widget_add_point (cie, icx, icy, icx_last, icy_last);
		icx_last = icx;
		icy_last = icy;
	}

	/* add data */
	gcm_cie_widget_compute_monochrome_color_location (cie, 380, &icx, &icy);
	gcm_cie_widget_add_point (cie, icx, icy, icx_last, icy_last);
}

static void
gcm_cie_widget_draw_tongue_outline (GcmCieWidget *cie, cairo_t *cr)
{
	guint wavelength;
	gdouble icx, icy;
	gdouble icx_last, icy_last;

	cairo_save (cr);
	cairo_set_line_width (cr, 2.0f);
	cairo_set_source_rgb (cr, 0.5f, 0.5f, 0.5f);

	/* get first co-ordinate */
	gcm_cie_widget_compute_monochrome_color_location (cie, 380, &icx_last, &icy_last);
	cairo_move_to (cr, icx_last, icy_last);

	/* this is fast path */
	for (wavelength = 380+1; wavelength <= 700; wavelength++) {

		/* get point */
		gcm_cie_widget_compute_monochrome_color_location (cie, wavelength, &icx, &icy);

		/* nothing to plot */
		if (icx == icx_last && icy == icy_last)
			continue;

		/* draw line */
		cairo_line_to (cr, icx, icy);

		icx_last = icx;
		icy_last = icy;
	}

	/* join bottom */
	cairo_close_path (cr);
	cairo_stroke_preserve (cr);
	cairo_set_line_width (cr, 1.0f);
	cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f);
	cairo_stroke (cr);

	cairo_restore (cr);
}

/**
 * gcm_cie_widget_xyz_to_rgb:
 *
 * Given an additive tricolor system CS, defined by the CIE x and y
 * chromaticities of its three primaries (z is derived trivially as
 * 1- (x+y)), and a desired chromaticity (XC, YC, ZC) in CIE space,
 * determine the contribution of each primary in a linear combination
 * which sums to the desired chromaticity. If the requested
 * chromaticity falls outside the Maxwell triangle (color gamut)
 * formed by the three primaries, one of the r, g, or b weights will
 * be negative.
 *
 * Caller can use gcm_cie_widget_constrain_rgb () to desaturate an outside-gamut
 * color to the closest representation within the available
 * gamut.
 **/
static void
gcm_cie_widget_xyz_to_rgb (GcmCieWidget *cie,
			   gdouble xc, gdouble yc, gdouble zc,
			   gdouble *r, gdouble *g, gdouble *b)
{
	gdouble xr, yr, zr, xg, yg, zg, xb, yb, zb;
	gdouble xw, yw, zw;
	gdouble rx, ry, rz, gx, gy, gz, bx, by, bz;
	gdouble rw, gw, bw;
	GcmCieWidgetPrivate *priv = cie->priv;

	xr = priv->red->x; yr = priv->red->y; zr = 1 - (xr + yr);
	xg = priv->green->x; yg = priv->green->y; zg = 1 - (xg + yg);
	xb = priv->blue->x; yb = priv->blue->y; zb = 1 - (xb + yb);

	xw = priv->white->x; yw = priv->white->y; zw = 1 - (xw + yw);

	/* xyz -> rgb matrix, before scaling to white-> */
	rx = yg*zb - yb*zg; ry = xb*zg - xg*zb; rz = xg*yb - xb*yg;
	gx = yb*zr - yr*zb; gy = xr*zb - xb*zr; gz = xb*yr - xr*yb;
	bx = yr*zg - yg*zr; by = xg*zr - xr*zg; bz = xr*yg - xg*yr;

	/* white scaling factors - dividing by yw scales the white luminance to unity */
	rw = (rx*xw + ry*yw + rz*zw) / yw;
	gw = (gx*xw + gy*yw + gz*zw) / yw;
	bw = (bx*xw + by*yw + bz*zw) / yw;

	/* xyz -> rgb matrix, correctly scaled to white-> */
	rx = rx / rw; ry = ry / rw; rz = rz / rw;
	gx = gx / gw; gy = gy / gw; gz = gz / gw;
	bx = bx / bw; by = by / bw; bz = bz / bw;

	/* rgb of the desired point */
	*r = rx*xc + ry*yc + rz*zc;
	*g = gx*xc + gy*yc + gz*zc;
	*b = bx*xc + by*yc + bz*zc;
}

/**
 * gcm_cie_widget_constrain_rgb:
 *
 * If the requested RGB shade contains a negative weight for one of
 * the primaries, it lies outside the color gamut accessible from
 * the given triple of primaries. Desaturate it by adding white,
 * equal quantities of R, G, and B, enough to make RGB all positive.
 **/
static gboolean
gcm_cie_widget_constrain_rgb (gdouble *r, gdouble *g, gdouble *b)
{
	gdouble w;

	/* amount of white needed is w = - min (0, *r, *g, *b) */
	w = (0 < *r) ? 0 : *r;
	w = (w < *g) ? w : *g;
	w = (w < *b) ? w : *b;
	w = - w;

	/* add just enough white to make r, g, b all positive. */
	if (w > 0) {
		*r += w; *g += w; *b += w;
		return TRUE; /* color modified to fit RGB gamut */
	}

	return FALSE; /* color within RGB gamut */
}

/**
 * gcm_cie_widget_gamma_correct:
 *
 * Transform linear RGB values to nonlinear RGB values.
 *
 * Rec. 709 is ITU-R Recommendation BT. 709 (1990)
 * ``Basic Parameter Values for the HDTV Standard for the Studio and for
 * International Programme Exchange'', formerly CCIR Rec. 709.
 *
 * For details see
 * http://www.inforamp.net/~poynton/ColorFAQ.html
 * http://www.inforamp.net/~poynton/GammaFAQ.html
 **/
static void
gcm_cie_widget_gamma_correct (GcmCieWidget *cie, gdouble *c)
{
	GcmCieWidgetPrivate *priv = cie->priv;

	if (priv->gamma == 0.0) {
		/* rec. 709 gamma correction. */
		gdouble cc = 0.018;
		if (*c < cc) {
			*c *= (1.099 * powf (cc, 0.45) - 0.099) / cc;
		} else {
			*c = 1.099 * powf (*c, 0.45) - 0.099;
		}
	} else {
		/* Nonlinear color = (Linear color)^ (1/gamma) */
		*c = powf (*c, 1.0/priv->gamma);
	}
}

static void
gcm_cie_widget_gamma_correct_rgb (GcmCieWidget *cie,
				  gdouble *r, gdouble *g, gdouble *b)
{
	gcm_cie_widget_gamma_correct (cie, r);
	gcm_cie_widget_gamma_correct (cie, g);
	gcm_cie_widget_gamma_correct (cie, b);
}

static void
gcm_cie_widget_draw_gamut_outline (GcmCieWidget *cie, cairo_t *cr)
{
	gdouble wx;
	gdouble wy;
	GcmCieWidgetPrivate *priv = cie->priv;

	cairo_save (cr);

	cairo_set_line_width (cr, 0.9f);
	cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f);

	gcm_cie_widget_map_to_display (cie, priv->red->x, priv->red->y, &wx, &wy);
	if (wx < 0 || wy < 0)
		goto out;
	cairo_move_to (cr, wx, wy);

	gcm_cie_widget_map_to_display (cie, priv->green->x, priv->green->y, &wx, &wy);
	if (wx < 0 || wy < 0)
		goto out;
	cairo_line_to (cr, wx, wy);

	gcm_cie_widget_map_to_display (cie, priv->blue->x, priv->blue->y, &wx, &wy);
	if (wx < 0 || wy < 0)
		goto out;
	cairo_line_to (cr, wx, wy);

	cairo_close_path (cr);
	cairo_stroke (cr);
out:
	cairo_restore (cr);
}

static void
gcm_cie_widget_draw_white_point_cross (GcmCieWidget *cie, cairo_t *cr)
{
	gdouble wx;
	gdouble wy;
	gdouble size;
	gdouble gap;
	GcmCieWidgetPrivate *priv = cie->priv;

	cairo_save (cr);

	/* scale the cross according the the widget size */
	size = priv->chart_width / 35.0f;
	gap = size / 2.0f;

	cairo_set_line_width (cr, 1.0f);

	/* choose color of cross */
	if (priv->red->x < 0.001 && priv->green->x < 0.001 && priv->blue->x < 0.001)
		cairo_set_source_rgb (cr, 1.0f, 1.0f, 1.0f);
	else
		cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f);

	gcm_cie_widget_map_to_display (cie, priv->white->x, priv->white->y, &wx, &wy);

	/* don't antialias the cross */
	wx = (gint) wx + 0.5f;
	wy = (gint) wy + 0.5f;

	/* left */
	cairo_move_to (cr, wx - gap, wy);
	cairo_line_to (cr, wx - gap - size, wy);
	cairo_stroke (cr);

	/* right */
	cairo_move_to (cr, wx + gap, wy);
	cairo_line_to (cr, wx + gap + size, wy);
	cairo_stroke (cr);

	/* top */
	cairo_move_to (cr, wx, wy - gap);
	cairo_line_to (cr, wx, wy - gap - size);
	cairo_stroke (cr);

	/* bottom */
	cairo_move_to (cr, wx, wy + gap);
	cairo_line_to (cr, wx, wy + gap + size);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
gcm_cie_widget_draw_line (GcmCieWidget *cie, cairo_t *cr)
{
	gdouble x, y;
	GcmCieWidgetPrivate *priv = cie->priv;
	GcmCieWidgetBufferItem *item;

	/* save for speed */
	gcm_cie_widget_get_min_max_tongue (cie);

	cairo_save (cr);
	for (y = 0; y < priv->chart_height; ++y) {

		/* get buffer data to se if there's any point rendering this line */
		item = g_ptr_array_index (priv->tongue_buffer, (guint)y);
		if (!item->valid)
			continue;

		for (x=item->min; x < item->max; x++) {

			gdouble cx, cy, cz;
			gdouble jr, jg, jb;
			gdouble r, g, b, mx;
			gdouble jmax;

			/* scale for display */
			gcm_cie_widget_map_from_display (cie, x, y, &cx, &cy);
			cz = 1.0 - (cx + cy);

			gcm_cie_widget_xyz_to_rgb (cie, cx, cy, cz, &jr, &jg, &jb);
			mx = 1.0f;

			/* Check whether the requested color is within the
			 * gamut achievable with the given color system. If
			 * not, draw it in a reduced intensity, interpolated
			 * by desaturation to the closest within-gamut color.
			 */
			if (gcm_cie_widget_constrain_rgb (&jr, &jg, &jb))
				mx = (1.0 * 3) / 4;

			/* Scale to max (rgb) = 1. */
			jmax = MAX (jr, MAX (jg, jb));
			if (jmax > 0) {
				jr = jr / jmax;
				jg = jg / jmax;
				jb = jb / jmax;
			}

			/* gamma correct from linear rgb to nonlinear rgb. */
			gcm_cie_widget_gamma_correct_rgb (cie, &jr, &jg, &jb);
			r = mx * jr;
			g = mx * jg;
			b = mx * jb;

			cairo_set_source_rgb (cr, r, g, b);
			/* convert to an integer to avoid antialiasing, which
			 * speeds things up significantly */
			cairo_rectangle (cr, (guint)x, (guint)y, 1, 1);
			cairo_fill (cr);
		}
	}

	cairo_restore (cr);

	/* overdraw lines with nice antialiasing */
	gcm_cie_widget_draw_tongue_outline (cie, cr);
	gcm_cie_widget_draw_gamut_outline (cie, cr);
}

static void
gcm_cie_widget_draw_bounding_box (cairo_t *cr, gint x, gint y, gint width, gint height)
{
	/* background */
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill (cr);

	/* solid outline box */
	cairo_rectangle (cr, x + 0.5f, y + 0.5f, width - 1, height - 1);
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

static void
gcm_cie_widget_draw_cie (GtkWidget *cie_widget, cairo_t *cr)
{
	GtkAllocation allocation;

	GcmCieWidget *cie = (GcmCieWidget*) cie_widget;
	g_return_if_fail (cie != NULL);
	g_return_if_fail (GCM_IS_CIE_WIDGET (cie));

	cairo_save (cr);

	/* make size adjustment */
	gtk_widget_get_allocation (cie_widget, &allocation);
	cie->priv->chart_height = allocation.height;
	cie->priv->chart_width = allocation.width;
	cie->priv->x_offset = cie->priv->chart_width / 18.0f;
	cie->priv->y_offset = cie->priv->chart_height / 20.0f;

	/* cie background */
	gcm_cie_widget_draw_bounding_box (cr, 0, 0, cie->priv->chart_width, cie->priv->chart_height);
	if (cie->priv->use_grid)
		gcm_cie_widget_draw_grid (cie, cr);

	gcm_cie_widget_draw_line (cie, cr);

	if (cie->priv->use_whitepoint)
		gcm_cie_widget_draw_white_point_cross (cie, cr);

	cairo_restore (cr);
}

static gboolean
gcm_cie_widget_draw (GtkWidget *cie, cairo_t *cr)
{
	gcm_cie_widget_draw_cie (cie, cr);
	return FALSE;
}

GtkWidget *
gcm_cie_widget_new (void)
{
	return g_object_new (GCM_TYPE_CIE_WIDGET, NULL);
}

