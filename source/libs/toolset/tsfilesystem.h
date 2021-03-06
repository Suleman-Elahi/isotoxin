#pragma once
#ifndef _INCLUDE_TSFILESYSTEM
#define _INCLUDE_TSFILESYSTEM

#define FNO_LOWERCASEAUTO 1 // lower case name (only case insensitive os, eg: windows)
#define FNO_REMOVECRAP 2    // removes .. and . and double slashes from path
#define FNO_NORMALIZE 4     // replace enemy_slash to native_slash
#define FNO_PARSENENV 8     // parse environment like %USER%, %APPDATA% and other %nnn%
#define FNO_FULLPATH  16    // build full path according to current directory
#define FNO_TRIMLASTSLASH 32
#define FNO_APPENDSLASH 64
#define FNO_MAKECORRECTNAME 128 // process only name - replace all incorrect filename symbols to _ (underscore)
#define FNO_SIMPLIFY (FNO_NORMALIZE|FNO_FULLPATH|FNO_LOWERCASEAUTO|FNO_REMOVECRAP)
#define FNO_SIMPLIFY_NOLOWERCASE (FNO_NORMALIZE|FNO_FULLPATH|FNO_REMOVECRAP)

namespace ts
{

#ifdef _WIN32
#define NATIVE_SLASH '\\'
#define NATIVE_SLASH_S "\\"
#define ENEMY_SLASH '/'
#endif
#ifdef _NIX
#define NATIVE_SLASH '/'
#define NATIVE_SLASH_S "/"
#define ENEMY_SLASH '\\'
#endif

INLINE bool __is_slash(const wchar c)
{
    return c == NATIVE_SLASH || c == ENEMY_SLASH;
}

aint  TSCALL get_exe_full_name( char *buf, aint buflen );
wstr_c  TSCALL get_exe_full_name();
void    TSCALL set_work_dir(wstr_c &wd, wstr_c *exename = nullptr);
inline void    set_work_dir()
{
	wstr_c wd;
	set_work_dir(wd);
}

void TSCALL parse_env(wstr_c &envstring); // or call fix_path( FNO_PARSEENV )
void  TSCALL fix_path(wstr_c &ipath, int fnoptions);
inline wstr_c  TSCALL fn_fix_path(const wsptr &ipath, int fnoptions)
{
    wstr_c p(ipath);
    fix_path(p, fnoptions);
    return p;
}

bool    TSCALL make_path(const wstr_c &path, int fnoptions);

enum scan_dir_e
{
    SD_CONTINUE,
    SD_TERMINATE,
    SD_STOP_SCAN_THIS_LEVEL
};

class scan_dir_file_descriptor_c
{
    wstr_c path_;
    wstr_c full_;
    wstr_c name_;
#ifdef _WIN32
    pwstr_c p_name_;
#endif
#ifdef _NIX
    pstr_c p_name_;
#endif
    uint64 sz_ = 0xffffffffffffffffull;
    uint64 tm_ = 0;

public:

    bool is_dir() const
    {
        return p_name_.is_empty();
    }

#ifdef _WIN32
    void prepare(const wstr_c &path, const pwstr_c &name);
#endif
#ifdef _NIX
    void prepare(const wstr_c &path, const pstr_c &name);
#endif
    wstr_c path() const
    {
        return path_;
    }

    wstr_c name()
    {
        if (name_.is_empty())
        {
#ifdef _WIN32
            name_.set(p_name_);
#endif
#ifdef _NIX
            name_ = from_utf8(p_name_);
#endif
        }
        return name_;
    }

    wstr_c fullname();
    uint64 size();
    uint64 modtime(); // modification time
};

typedef fastdelegate::FastDelegate< scan_dir_e(int , scan_dir_file_descriptor_c &) > SCAN_DIR_CALLBACK_H;
void TSCALL scan_dir(const wstr_c &path, SCAN_DIR_CALLBACK_H cb);

namespace internals
{
    template<typename CB> struct scandirproxy_s
    {
        scandirproxy_s(const CB &cb) :cb(cb) {}
        const CB &cb;
        scan_dir_e scanh(int lv, scan_dir_file_descriptor_c &d)
        {
            return cb(lv, d);
        }

    private:
        scandirproxy_s(const scandirproxy_s&) UNUSED;
        scandirproxy_s(scandirproxy_s &&) UNUSED;
        void operator=(const scandirproxy_s&) UNUSED;
        void operator=(scandirproxy_s &&) UNUSED;

    };
}

template<typename CB> void scan_dir_t(const wstr_c &path, const CB &cb)
{
    internals::scandirproxy_s<CB> p(cb);
    scan_dir(path, DELEGATE(&p, scanh));
}

void    TSCALL del_dir(const wstr_c &path);
void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone, const wsptr &skip = CONSTWSTR(".svn;.hg;.git"));

bool    TSCALL is_dir_exists(const wstr_c &path);

bool TSCALL is_file_exists(const wsptr &fname);
bool TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname);

uint64 TSCALL get_free_space( const wstr_c &path );

struct filefilter_s : public movable_flag<true>
{
    ts::wstr_c wildcard;
    ts::wstr_c desc;
};

struct filefilters_s
{
    filefilters_s():filters(nullptr, 0) {}
    filefilters_s(const filefilter_s *f, ts::aint cnt, int def = -1):filters(f,cnt), index(def) {}
    array_wrapper_c<const filefilter_s> filters;
    aint index = -1;
    const wchar *defwildcard() const
    {
        if (index < 0 || index >= filters.size()) return nullptr;
        return (filters.begin() + index)->wildcard.cstr();
    }
};

#define ATTR_ANY -1
#define ATTR_DIR SETBIT(0)

bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, int attributes, int skip_attributes = 0, bool full_names = false);

void TSCALL fn_split( const wsptr &full_name, wstr_c &name, wstr_c &ext );
template<class CORE> wstr_c	TSCALL fn_join(const str_t<wchar, CORE> &path, const str_t<wchar, CORE> &name)
{
	if (path.is_empty()) return name;
	if (path.get_last_char() == NATIVE_SLASH || path.get_last_char() == ENEMY_SLASH)
	{
		if ( name.get_char(0) == NATIVE_SLASH || name.get_char(0) == ENEMY_SLASH ) return wstr_c(path).append(name.as_sptr().skip(1));
		return wstr_c(path).append(name);
	}
	if ( name.get_char(0) == NATIVE_SLASH || name.get_char(0) == ENEMY_SLASH ) return wstr_c( path ).append( name );
	return wstr_c( path ).append_char( NATIVE_SLASH ).append( name );

}
template<class CORE> wstr_c TSCALL fn_join(const str_t<wchar, CORE> &path, const wsptr &name)
{
	if (path.get_last_char() == NATIVE_SLASH || path.get_last_char() == ENEMY_SLASH)
	{
		if ( name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH ) return wstr_c(path).append(name.skip(1));
		return wstr_c(path,name);
	}
	if ( name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH ) return wstr_c( path ).append( name );
	return wstr_c( path ).append_char( NATIVE_SLASH ).append( name );
}
INLINE wstr_c fn_join(const wsptr &ipath, const wsptr &name)
{
    return fn_join<str_core_part_c<ZSTRINGS_WIDECHAR> >( pwstr_c(ipath), name );
}

template<class CORE> wstr_c TSCALL fn_join(const str_t<wchar, CORE> &path, const wsptr &path1, const wsptr &name)
{
	wstr_c t = fn_join(path, path1);
	if (t.get_last_char() == NATIVE_SLASH || t.get_last_char() == ENEMY_SLASH)
	{
		if (name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH) return t.append(name.skip(1));
		return t.append(name);
	}
	if (name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH) return t.append(name);
	return t.append_char( NATIVE_SLASH ).append( name );
}
INLINE wstr_c TSCALL fn_get_name_with_ext(const wsptr &name)
{
    int i = pwstr_c(name).find_last_pos_of(CONSTWSTR("/\\"));
	if (i < 0) i = 0; else ++i;
	return wstr_c(name.skip(i));
}
//INLINE wstr_c TSCALL fn_get_short_name(const wsptr &name)
//{
//	return fn_get_name_with_ext(name).case_down();
//}

template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_autoquote(const str_t<TCHARACTER> &name);
wstr_c TSCALL fn_get_name(const wsptr &name);
wstr_c TSCALL fn_get_ext(const wsptr &name);
wstr_c TSCALL fn_get_path(const wstr_c &name); // with ending slash

template<typename TCHARACTER> bool TSCALL fn_mask_match( const sptr<TCHARACTER> &fn, const sptr<TCHARACTER> &mask );

template <class TCHARACTER> bool TSCALL parse_text_file(const wsptr &filename, strings_t<TCHARACTER>& text, bool from_utf8 = false);

wstr_c TSCALL fn_change_name(const wsptr &full, const wsptr &name);
wstr_c TSCALL fn_change_ext(const wsptr &full, const wsptr &ext);

inline wstr_c fn_change_name_ext(const wstr_c &full, const wsptr &name, const wsptr &ext)
{
    int i = full.find_last_pos_of(CONSTWSTR("/\\")) + 1;
    return wstr_c(wsptr(full.cstr(), i)).append(name).append_char('.').append(ext);
}

inline wstr_c fn_change_name_ext(const wstr_c &full, const wsptr &nameext)
{
    int i = full.find_last_pos_of(CONSTWSTR("/\\")) + 1;
    return wstr_c(wsptr(full.cstr(), i)).append(nameext);
}

wstr_c INLINE scan_dir_file_descriptor_c::fullname()
{
    if (full_.is_empty())
        full_ = fn_join(path_, name());
    return full_;
}


class enum_files_c
{
#ifdef _WIN32
    uint8 data[ 768 ];
#endif
#ifdef _NIX
    uint8 data[ARCHBITS];
#endif

    bool prepare_file();
    void next_int();

public:

    enum_files_c( const wstr_c &base, const wstr_c &path, const wstr_c &wildcard );
    ~enum_files_c();
    operator bool() const;

    void next();

    void operator++() { next(); }
    void operator++( int ) { ++( *this ); }

    const wstr_c &operator* () const;
    const wstr_c *operator->() const;

    bool is_folder() const;
};

template<class RCV, class STRCORE> bool enum_files(const str_t<wchar, STRCORE> &base, RCV &pred, const str_t<wchar, STRCORE> &path = str_t<wchar, STRCORE>(), const wsptr &wildcard = CONSTWSTR("*.*"))
{
    enum_files_c ef( base, path, ts::wstr_c(wildcard) );
    for ( ;ef; ++ef )
    {
        if ( ef.is_folder() )
        {
            if ( !enum_files( base, pred, *ef, wildcard ) )
                return false;
        }
        else
            if ( !pred( base, *ef ) ) return true;
    }

	return true;
}


bool TSCALL check_write_access(const wsptr &path);

bool TSCALL kill_file(const wsptr &path);

enum copy_rslt_e
{
    CRSLT_OK,
    CRSLT_ACCESS_DENIED,
    CRSLT_FAIL
};
copy_rslt_e TSCALL copy_file( const wsptr &existingfn, const wsptr &newfn );
bool TSCALL ren_or_move_file( const wsptr &existingfn, const wsptr &newfn );

bool TSCALL is_64bit_os();

} // namespace ts


#endif