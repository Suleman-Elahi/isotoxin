#include "toolset.h"
#include "internal/platform.h"

namespace ts
{

    INLINE ivec4 TSCOLORtoVec4(TSCOLOR color)
    {
        return ivec4(RED(color), GREEN(color), BLUE(color), ALPHA(color));
    }


INLINE void writePixelBlended(TSCOLOR &dst, const ivec4 &srcColor)
{
	ivec4 dstColor(TSCOLORtoVec4(dst));
	dstColor.rgb() = srcColor.rgb()*srcColor.a + dstColor.rgb()*(256-srcColor.a);
	dstColor.a = 255*256 - (255-dstColor.a)*(255-srcColor.a);
	dst = ((dstColor.r & 0xFF00) << 8) | (dstColor.g & 0xFF00) | (dstColor.b >> 8) | ((dstColor.a & 0xFF00) << 16);
}

INLINE void write_pixel(TSCOLOR &dst, TSCOLOR src, uint8 aa)
{
    if (aa == 0) return;
    TSCOLOR c = dst;

    extern uint8 ALIGN(256) multbl[256][256];

    uint8 a = multbl[aa][ ALPHA(src) ];
    uint16 not_a = 255 - a;

    uint B = multbl[not_a][BLUE(c)] + multbl[a][BLUE(src)];
    uint G = multbl[not_a][GREEN(c)] + multbl[a][GREEN(src)];
    uint R = multbl[not_a][RED(c)] + multbl[a][RED(src)];
    uint A = multbl[not_a][ALPHA(c)] + a;

    dst = B | (G << 8) | (R << 16) | (A << 24);
}

#if LCD_RENDER_MODE
INLINE void write_pixel_lcd(TSCOLOR &dst, TSCOLOR src, const uint8 *glyph_rgb)
{
    TSCOLOR c = dst;

    extern uint8 ALIGN(256) multbl[256][256];

#define CMUL(a,b) multbl[a][b]

    auto grayscale = [](const uint8 *glyph_rgb)->int
    {
        return ts::lround(float(glyph_rgb[0]) * 0.0722f + float(glyph_rgb[1]) * 0.7152f + float(glyph_rgb[2]) * 0.2126f);
    };


    uint8 a = CMUL(grayscale(glyph_rgb ), ALPHA(src));
    uint16 not_a = 255 - a;


    uint B = CMUL(not_a, BLUE(c)) + CMUL(a, CMUL(glyph_rgb[0], BLUE(src)));
    uint G = CMUL(not_a, GREEN(c)) + CMUL(a, CMUL(glyph_rgb[1], GREEN(src)));
    uint R = CMUL(not_a, RED(c)) + CMUL(a, CMUL(glyph_rgb[2], RED(src)));
    uint A = CMUL(not_a, ALPHA(c)) + a;

#undef CMUL

    dst = B | (G << 8) | (R << 16) | (A << 24);
}
#endif

static const ALIGN(16) uint8 prepare_4alphas[16] = { 255, 0, 255, 1, 255, 2, 255, 3, 255, 0, 255, 1, 255, 2, 255, 3 };
static const ALIGN(16) uint16 add1[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static const ALIGN(16) uint8 invhi[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0 };

static const ALIGN(16) uint8 prepare_a1r[16] = { 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 2, 3 };
static const ALIGN(16) uint8 prepare_a2r[16] = { 4, 5, 4, 5, 4, 5, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7 };

static const ALIGN(16) uint8 prepare_a1l[16] = { 8, 9, 8, 9, 8, 9, 8, 9, 10, 11, 10, 11, 10, 11, 10, 11 };
static const ALIGN(16) uint8 prepare_a2l[16] = { 12, 13, 12, 13, 12, 13, 12, 13, 14, 15, 14, 15, 14, 15, 14, 15 };

namespace sse_consts
{
extern ALIGN(16) uint8 preparetgtc_1[16];
extern ALIGN(16) uint8 preparetgtc_2[16];
};

static void write_row_sse( uint8 *dst_argb, const uint8 *src_argb, int w, const uint16 * color )
{
    for ( ; w > 0; w -= 4, dst_argb += 16, src_argb += 4 )
    {
        __m128i pta1 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_1 );
        __m128i pta2 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_2 );
        __m128i t6 = _mm_load_si128( ( const __m128i * )color );
        __m128i t7 = _mm_load_si128( ( const __m128i * )( color + 8 ) );
        __m128i t3 = _mm_load_si128( ( const __m128i * )prepare_4alphas );
        __m128i a1r = _mm_load_si128( ( const __m128i * )prepare_a1r );
        __m128i a1l = _mm_load_si128( ( const __m128i * )prepare_a1l );
        __m128i a2r = _mm_load_si128( ( const __m128i * )prepare_a2r );
        __m128i a2l = _mm_load_si128( ( const __m128i * )prepare_a2l );
        __m128i tadd1 = _mm_load_si128( ( const __m128i * )add1 );
        __m128i tinvhi = _mm_load_si128( ( const __m128i * )invhi );

        __m128i tt5 = _mm_add_epi16( _mm_xor_si128( _mm_mulhi_epu16( _mm_shuffle_epi8( _mm_cvtsi32_si128( *(uint32 *)src_argb ), t3 ), t7 ), tinvhi ), tadd1);

        __m128i t4 = _mm_lddqu_si128( ( const __m128i * )dst_argb );
        
        _mm_storeu_si128( ( __m128i * )dst_argb,
            _mm_packus_epi16( _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t4, pta1 ), _mm_shuffle_epi8( tt5, a1l ) ), _mm_mulhi_epu16( _mm_shuffle_epi8( tt5, a1r ), t6 ) ),
                _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t4, pta2 ), _mm_shuffle_epi8( tt5, a2l ) ), _mm_mulhi_epu16( _mm_shuffle_epi8( tt5, a2r ), t6 ) ) ) );
    }
}


static void __special_blend(uint8 *dst_argb, int dst_pitch, const uint8 *src_alpha, int src_pitch, int width, int height, TSCOLOR color)
{
#if LCD_RENDER_MODE

    for (; height; height--, src_alpha += src_pitch, dst_argb += dst_pitch)
        for (int i = 0; i < width; ++i)
            write_pixel_lcd(((TSCOLOR*)dst_argb)[i], color, src_alpha + (i<<2));

#else

    if (CCAPS(CPU_SSSE3))
    {
        int w = width & ~3;

        if (w)
        {
            uint16 ap1 = 1 + ALPHA(color);
            ALIGN(16) ts::uint16 precolor[16] =
            { 
                BLUEx256(color), GREENx256(color), REDx256(color), (255 << 8), BLUEx256(color), GREENx256(color), REDx256(color), (255 << 8),
                ap1, ap1, ap1, ap1, ap1, ap1, ap1, ap1
            };

            uint8 * dst_sse = dst_argb;
            const uint8 * src_sse = src_alpha;

            for (int y = 0; y < height; ++y, dst_sse += dst_pitch, src_sse += src_pitch)
                write_row_sse(dst_sse, src_sse, w, precolor);
        }

        if (int ost = width & 3)
        {
            src_alpha += w;
            dst_argb += w * 4;

            for (; height; height--, src_alpha += src_pitch, dst_argb += dst_pitch)
                for (int i = 0; i < ost; ++i)
                    write_pixel(((TSCOLOR*)dst_argb)[i], color, src_alpha[i]);
        }

        return;
    }

    for (; height; height--, src_alpha += src_pitch, dst_argb += dst_pitch)
        for (int i = 0; i < width; ++i)
            write_pixel(((TSCOLOR*)dst_argb)[i], color, src_alpha[i]);
#endif
}


bitmap_c text_rect_c::make_bitmap()
{
    ASSERT( !flags.is( F_INVALID_TEXTURE | F_INVALID_GLYPHS ) );
    int n = prepare_textures( size );
    const bitmap_c **tarr = (const bitmap_c **)_alloca( sizeof( bitmap_c * ) * n );
    get_textures(tarr);

    ts::bitmap_c bmp;
    bmp.create_ARGB( size );

    int y = 0;
    for( int i=0;i<n;++i )
    {
        const bitmap_c *b = tarr[ i ];
        ASSERT( b->info().sz.x >= size.x );
        int h = tmin( size.y - y, b->info().sz.y );
        bmp.copy( ts::ivec2( 0, y ), ivec2( size.x, h ), b->extbody(), ivec2(0) );
        y += h;
    }
    return bmp;
};

bool text_rect_c::draw_glyphs(uint8 *dst_, int width, int height, int pitch, const array_wrapper_c<const glyph_image_s> &glyphs, const ivec2 &offset, bool prior_clear)
{
    // sorry guys, russian description of algorithm

	/*
      TAV:
      ��� ��������� ������ � �������� ������������ ����������� �������� ���������. �.�. ����� ����� ���������� ������ ���� �����,
	  � ��������� ��� ������������� ����������� ����� ��� ��������������� � �������� �������� � �����, �� ������ �������� �����
	  ������ ���� ����� ��� ������������ ������� ������� (����. �������� ����, ���� ����� > 0, � ����� ������������ �����) ������.
	  ����������, ����� ���������� ��������� ��� ������������ ������������ ����������� ���� �������� ������, �� �������, ��
	  ����������� ���������� (ALPHA, 1-ALPHA) ��� ����������� �����������. ���� ����������� ������, ������� ����������� ��������
	  ������ ���������� ���������� � ���� ��������.

	--- Rationale ---
	������: ������������� ����� � ��������� � ������ ��� ��������� �� ������� ����������� background (�� ����������),
			� ����� ����� ����������� �������� � ������ � �������� �� �� ����� � ������.
	d1 - background (������� �� ��������)
	s1 - ��������
	s2 - �����
	�� ����������� ������� �������� (ALPHA, 1-ALPHA) ���� �� background ��� ��������, �������� �������� ����� (d3) ����� ���� �� ����� ���:
	d2 = s1*s1a+d1*(1-s1a)
	d3 = s2*s2a+d2*(1-s2a) = s2*s2a + (s1*s1a+d1*(1-s1a)) * (1-s2a)
		= (_ s2*s2a + s1*s1a*(1-s2a) _)  +  d1 * (_ (1-s1a)*(1-s2a) _)
	����� ����� ��������� �� RGB, � ������ - �� �. ��� ����� ����� ������� ���������� ���� �� �����. �.�. �������� d1 �� ��������, ����������� ��� ������.
	���������� ����� dst �������� (0,0,0,255). �������� ������� � ���� ����� (dst) �� ��������� ������ (src,alpha) ����� �� �������:
	RGB = src*alpha + dst*(1-alpha)
	A = A*(1-alpha)
	�.�. � ������ dst �� �������� �� ���-�� ��������� ������, � RGB ����� �������� �����, � � A - �����.
	����� ��������� ������� ���������� ����� �� GPU, ����� �������� ����� (ONE, ALPHA).*/

	//������� �������������� ����� ����������, �.�. ��������� ��� ������ (�� (0,0,0,255), �.�. ����� ����� �������� � ����� ���������������).

	uint8 *dst = dst_;
	if (prior_clear)
		for (int h=height; h; h--, dst+=pitch)
			memset(dst, 0, width*sizeof(TSCOLOR));

    int g = 0;
    if (glyphs.size() && glyphs[0].pixels == nullptr)
    {
        // special glyph changing rendering order
        if ( glyphs[0].outline_index > 0 )
        {
            draw_glyphs(dst_, width, height, pitch, glyphs.subarray(glyphs[0].outline_index), offset, false);
            return draw_glyphs(dst_, width, height, pitch, glyphs.subarray(1, glyphs[0].outline_index), offset, false);
        }
        g = 1;
    }

	// now render glyphs, clipping them by buffer area
    bool rectangles = false;
	for (; g<glyphs.size(); g++)
	{
		const glyph_image_s &glyph = glyphs[g];
        if (glyph.pixels < GTYPE_DRAWABLE)
        {
            if ( glyph.pixels == GTYPE_BGCOLOR_MIDDLE )
            {
                // draw bg
                ivec2 pos( glyph.pos() );  pos += offset; // glyph rendering position

                ivec2 clipped_size( glyph.width, glyph.height );
                // clipping
                if (pos.x < 0) clipped_size.x += pos.x, pos.x = 0;
                if (pos.y < 0) clipped_size.y += pos.y, pos.y = 0;
                if (pos.x + clipped_size.x > width) clipped_size.x = width - pos.x;
                if (pos.y + clipped_size.y > height) clipped_size.y = height - pos.y;

                if (clipped_size.x <= 0 || clipped_size.y <= 0) continue; // fully clipped - skip

                dst = dst_ + pos.x * sizeof(TSCOLOR) + pos.y * pitch;

                ts::TSCOLOR col = glyph.color;

                img_helper_overfill(dst, imgdesc_s(clipped_size, 32, (int16)pitch), col);

                /*
                for (; clipped_height; clipped_height--, dst += pitch)
                    for (int i = 0; i < clipped_width; i++)
                        ((TSCOLOR*)dst)[i] = ALPHABLEND_PM(((TSCOLOR*)dst)[i], col);
                */

                continue;
            }

            rectangles |= (glyph.pixels == GTYPE_RECTANGLE);
            continue; // pointer value < 16 means special glyph. Special glyph should be skipped while rendering
        }

#if LCD_RENDER_MODE
        const int srcbpp = 4; // bytes per pixel
#else
        int srcbpp = glyph.color ? 1 : 4; // bytes per pixel
#endif
        bool srcglyph = glyph.color != 0;
		ivec4 glyphColor = TSCOLORtoVec4(glyph.color);
		ivec2 pos( glyph.pos() );  pos += offset; // glyph rendering position
        ts::TSCOLOR gcol = glyph.color;

		// draw underline
		if (glyph.thickness > 0)
		{
			ivec2 start = pos + ivec2(glyph.start_pos());
			float thick = (tmax(glyph.thickness, 1.f) - 1)*.5f;
			int d = lceil(thick);
			start.y -= d;
			ivec2 end = tmin(ivec2(width, height), start + ivec2(glyph.length, d*2 + 1));
			start = tmax(ivec2(0,0), start);
			if (end.x > start.x && end.y > start.y)
			{
				dst = dst_ + start.y * pitch;
				ivec4 c = srcglyph ? glyphColor : ivec4(255);
				ivec4 ca(c.rgb(), ts::lround(c.a*(1 + thick - d)));
				// begin
				for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], ca);
				start.y++, dst += pitch;
				// middle
				for (; start.y<end.y-1; start.y++, dst += pitch)
					for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], c);
				// tail
				if (start.y<end.y)
					for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], ca);
			}
		}

		// draw glyph
		int clipped_width  = glyph.width;
		int clipped_height = glyph.height;
		const uint8 *src = glyph.pixels;
		// clipping
		if (pos.x < 0) src -= pos.x*srcbpp    , clipped_width  += pos.x, pos.x = 0;
		if (pos.y < 0) src -= pos.y*glyph.pitch, clipped_height += pos.y, pos.y = 0;
		if (pos.x + clipped_width  > width ) clipped_width  = width  - pos.x;
		if (pos.y + clipped_height > height) clipped_height = height - pos.y;

		if (clipped_width <= 0 || clipped_height <= 0) continue; // fully clipped - skip

		dst = dst_ + pos.x * sizeof(TSCOLOR) + pos.y * pitch;

        if (srcglyph)
        {
            __special_blend( dst, pitch, src, glyph.pitch, clipped_width, clipped_height, gcol );

            //for (; clipped_height; clipped_height--, src += glyphPitch, dst += pitch)
            //    for (int i = 0; i < clipped_width; i++)
            //        write_pixel( ((TSCOLOR*)dst)[i], gcol, src[i] );
        } else
        {
            aint glyph_pitch = glyph.pitch;
            for (; clipped_height; clipped_height--, src += glyph_pitch, dst += pitch)
                for (int i = 0; i < clipped_width; i++)
                    ((TSCOLOR*)dst)[i] = ALPHABLEND_PM( ((TSCOLOR*)dst)[i], ((TSCOLOR*)src)[i] );
        }

	}
    return rectangles;
}


text_rect_c::~text_rect_c()
{
}

void text_rect_c::update_rectangles( ts::ivec2 &offset, rectangle_update_s * updr )
{
    offset += updr->offset;
    for (const glyph_image_s &gi : glyphs().array())
        if (gi.pixels == (uint8 *)1)
            updr->updrect(updr->param, gi.pitch, ivec2(gi.pos().x + offset.x, gi.pos().y + offset.y));
}

void text_rect_c::update_rectangles( rectangle_update_s * updr )
{
    ASSERT(!is_dirty());
    ivec2 toffset = get_offset();
    update_rectangles(toffset, updr);
}

void text_rect_c::render_texture(rectangle_update_s * updr)
{
    if (glyphs().count() == 0) { textures_no_need(); return; }
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
	if (!CHECK(size.x > 0 && size.y > 0)) return;
    int n = prepare_textures( size );
    const bitmap_c **tarr = (const bitmap_c **)_alloca( sizeof( bitmap_c * ) * n );
    get_textures( tarr );
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    ivec2 toffset = get_offset();
    ivec2 woffset( toffset );

    int addh = 0;
    bool updrf = false;
    for ( int i=0;i<n;++i )
    {
        bitmap_c *t = const_cast<bitmap_c *>(tarr[ i ]);
        ASSERT( t->info().sz.x >= size.x );
        int ih = t->info().sz.y;
        int h = tmin( size.y - addh, ih );
        updrf |= draw_glyphs( (uint8*)t->body(), size.x, h, t->info().pitch, glyphs().array(), woffset );
        woffset.y -= ih;
        addh += ih;
    }

    if ( updrf && updr )
        update_rectangles( toffset, updr );
}

void text_rect_c::render_texture( rectangle_update_s * updr, fastdelegate::FastDelegate< void (bitmap_c&, int y, const ivec2 &size) > clearp )
{
    if (glyphs().count() == 0) return textures_no_need();
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
    if (!CHECK(size.x > 0 && size.y > 0)) return;
    int n = prepare_textures( size );
    const bitmap_c **tarr = (const bitmap_c **)_alloca( sizeof( bitmap_c * ) * n );
    get_textures( tarr );
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    ivec2 toffset = get_offset();
    ivec2 woffset( toffset );

    int addh = 0;
    bool updrf = false;
    for ( int i = 0; i < n; ++i )
    {
        bitmap_c *t = const_cast<bitmap_c *>( tarr[ i ] );
        ASSERT( t->info().sz.x >= size.x );
        int ih = t->info().sz.y;
        int h = tmin( size.y - addh, ih );

        clearp( *t, addh, ivec2(size.x, h) );

        updrf |= draw_glyphs( (uint8*)t->body(), size.x, h, t->info().pitch, glyphs().array(), woffset, false );
        woffset.y -= ih;
        addh += ih;
    }

    if ( updrf && updr )
        update_rectangles( toffset, updr );
}

bool text_rect_c::set_text(const wstr_c &text_, CUSTOM_TAG_PARSER ctp, bool do_parse_and_render_texture)
{
    if ( size >> ivec2(0) )
        prepare_textures( size );

    bool dirty = flags.is(F_INVALID_SIZE) || flags.is( F_INVALID_TEXTURE ) || flags.is(F_DIRTY);

	if (dirty || !text.equals(text_))
	{
        flags.set(F_DIRTY|F_INVALID_TEXTURE);
		text = text_;
		if (do_parse_and_render_texture && (*font)) 
            parse_and_render_texture(nullptr, ctp); 
        return true;
	}
    return false;
}

void text_rect_c::parse_and_render_texture(rectangle_update_s * updr, CUSTOM_TAG_PARSER ctp, bool do_render)
{
	glyphs().clear();
	int f = flags & (TO_WRAP_BREAK_WORD | TO_HCENTER | TO_LASTLINEADDH | TO_FORCE_SINGLELINE | TO_END_ELLIPSIS | TO_LINE_END_ELLIPSIS);
    flags.clear(F_INVALID_GLYPHS);
	lastdrawsize = parse_text(text, size.x-ui_scale(margins_lt.x)-ui_scale(margin_right), ctp, &glyphs(), default_color, (*font), f, size.y - ui_scale(margins_lt.y));
	text_height = lastdrawsize.y + ui_scale(margins_lt.y);
	lastdrawsize.x += ui_scale(margins_lt.x) + ui_scale(margin_right);
    lastdrawsize.y = text_height;
	if (do_render) render_texture(updr);
}

ivec2 text_rect_c::calc_text_size(int maxwidth, CUSTOM_TAG_PARSER ctp) const
{
    int w = maxwidth; if (w < 0) w = 16384;
    int f = flags & (TO_WRAP_BREAK_WORD | TO_HCENTER | TO_LASTLINEADDH | TO_FORCE_SINGLELINE | TO_END_ELLIPSIS | TO_LINE_END_ELLIPSIS);
    ts::ivec2 sz = parse_text(text, w-ui_scale(margins_lt.x)-ui_scale(margin_right), ctp, nullptr, default_color, (*font), f, 0);

    return sz + ts::ivec2(ui_scale(margins_lt.x) + ui_scale(margin_right), margins_lt.y);
}

ivec2 text_rect_c::calc_text_size( const font_desc_c& f, const wstr_c& t, int maxwidth, uint flgs, CUSTOM_TAG_PARSER ctp ) const
{
    return parse_text(t, maxwidth, ctp, nullptr, ARGB(0,0,0), f, flgs, 0);
}

} // namespace ts
