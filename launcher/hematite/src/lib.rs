use qobject::QString;

/// The bridge definition for our QObject
#[cxx::bridge]
pub mod qobject {

    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qstringlist.h");
        /// An alias to the QString type
        type QString = cxx_qt_lib::QString;
    }

    extern "Rust" {
    }
}

