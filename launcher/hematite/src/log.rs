use std::{cell::RefCell, fmt};

use tracing::span;
use tracing_subscriber::{
    field::VisitOutput,
    fmt::{
        format::{Compact, DefaultVisitor},
        FormatFields, FormattedFields,
    },
    layer::Context,
};

#[cxx::bridge]
pub mod ffi {

    #[namespace = "prism::hematite::log"]
    pub enum Level {
        /// The "trace" level.
        ///
        /// Designates very low priority, often extremely verbose, information.
        TRACE = 0,
        /// The "debug" level.
        ///
        /// Designates lower priority information.
        DEBUG = 1,
        /// The "info" level.
        ///
        /// Designates useful information.
        INFO = 2,
        /// The "warn" level.
        ///
        /// Designates hazardous situations.
        WARN = 3,
        /// The "error" level.
        ///
        /// Designates very serious errors.
        ERROR = 4,
    }

    #[namespace = "prism::hematite::log"]
    unsafe extern "C++" {
        include!("hematite-static/log.h");

        fn debug(file: &str, line: i32, function: &str, msg: &str);
        fn info(file: &str, line: i32, function: &str, msg: &str);
        fn warn(file: &str, line: i32, function: &str, msg: &str);
        fn critical(file: &str, line: i32, function: &str, msg: &str);
    }

    #[namespace = "prism::hematite::log"]
    extern "Rust" {
        fn setup_rust_tracing_qdebug(level: Level);
    }
}

fn setup_rust_tracing_qdebug(level: ffi::Level) {
    use tracing_subscriber::prelude::*;
    let level = match level {
        ffi::Level::TRACE => tracing::Level::TRACE,
        ffi::Level::DEBUG => tracing::Level::DEBUG,
        ffi::Level::INFO => tracing::Level::INFO,
        ffi::Level::WARN => tracing::Level::WARN,
        ffi::Level::ERROR => tracing::Level::ERROR,
        _ => tracing::Level::ERROR,
    };
    tracing_subscriber::registry()
        .with(QDebugLayer::new(level))
        .init();
    tracing::info!(
        rust_enabled = true,
        "rust tracing qDebug subscriber registered",
    );
}

pub struct QDebugLayer {
    level: tracing::Level,
    fmt_fields: tracing_subscriber::fmt::format::DefaultFields,
}

impl QDebugLayer {
    pub fn new(level: tracing::Level) -> Self {
        QDebugLayer {
            level,
            fmt_fields: tracing_subscriber::fmt::format::DefaultFields::default(),
        }
    }

    fn send_message(&self, event: &tracing::Event<'_>, msg: &str) {
        let file = event.metadata().file().unwrap_or("unknown");
        let line = event.metadata().line().unwrap_or_default() as i32;
        let target = event
            .metadata()
            .module_path()
            .unwrap_or(event.metadata().target());
        eprintln!("target: {target}");
        match *event.metadata().level() {
            tracing::Level::WARN => ffi::warn(file, line, target, msg),
            tracing::Level::INFO => ffi::info(file, line, target, msg),
            tracing::Level::ERROR => ffi::critical(file, line, target, msg),
            _ => ffi::debug(file, line, target, msg),
        }
    }
}

macro_rules! with_event_from_span {
    ($id:ident, $span:ident, $($field:literal = $value:expr),*, |$event:ident| $code:block) => {
        let meta = $span.metadata();
        let cs = meta.callsite();
        let fs = tracing::field::FieldSet::new(&[$($field),*], cs);
        #[allow(unused)]
        let mut iter = fs.iter();
        let v = [$(
            (&iter.next().unwrap(), ::core::option::Option::Some(&$value as &dyn tracing::field::Value)),
        )*];
        let vs = fs.value_set(&v);
        let $event = tracing::Event::new_child_of($id, meta, &vs);
        $code
    };
}

impl<S> tracing_subscriber::Layer<S> for QDebugLayer
where
    S: tracing::Subscriber + for<'a> tracing_subscriber::registry::LookupSpan<'a>,
{
    fn register_callsite(
        &self,
        metadata: &'static tracing::Metadata<'static>,
    ) -> tracing::subscriber::Interest {
        if self.level >= *metadata.level() {
            tracing::subscriber::Interest::always()
        } else {
            tracing::subscriber::Interest::never()
        }
    }

    fn enabled(
        &self,
        metadata: &tracing::Metadata<'_>,
        _ctx: tracing_subscriber::layer::Context<'_, S>,
    ) -> bool {
        self.level >= *metadata.level()
    }

    fn max_level_hint(&self) -> Option<tracing_subscriber::filter::LevelFilter> {
        Some(self.level.into())
    }

    fn on_new_span(
        &self,
        attrs: &span::Attributes<'_>,
        id: &span::Id,
        ctx: tracing_subscriber::layer::Context<'_, S>,
    ) {
        let span = ctx.span(id).expect("Span not found, this is a bug");
        let mut extensions = span.extensions_mut();

        if extensions
            .get_mut::<FormattedFields<tracing_subscriber::fmt::format::Compact>>()
            .is_none()
        {
            let mut fields =
                FormattedFields::<tracing_subscriber::fmt::format::Compact>::new(String::new());
            if self
                .fmt_fields
                .format_fields(fields.as_writer(), attrs)
                .is_ok()
            {
                extensions.insert(fields);
            } else {
                eprintln!(
                    "[prism-tracing-subscriber] Unable to format the following event, ignoring: {:?}",
                    attrs
                );
            }
        }

        with_event_from_span!(id, span, "message" = "new", |event| {
            drop(extensions);
            drop(span);
            self.on_event(&event, ctx);
        });
    }

    fn on_record(
        &self,
        id: &span::Id,
        values: &span::Record<'_>,
        ctx: tracing_subscriber::layer::Context<'_, S>,
    ) {
        let span = ctx.span(id).expect("Span not found, this is a bug");
        let mut extensions = span.extensions_mut();
        if let Some(fields) =
            extensions.get_mut::<FormattedFields<tracing_subscriber::fmt::format::DefaultFields>>()
        {
            let _ = self.fmt_fields.add_fields(fields, values);
            return;
        }

        let mut fields =
            FormattedFields::<tracing_subscriber::fmt::format::Format<Compact>>::new(String::new());
        if self
            .fmt_fields
            .format_fields(fields.as_writer(), values)
            .is_ok()
        {
            extensions.insert(fields);
        }
    }

    fn on_enter(&self, id: &span::Id, ctx: tracing_subscriber::layer::Context<'_, S>) {
        let span = ctx.span(id).expect("Span not found, this is a bug");
        let extensions = span.extensions_mut();

        with_event_from_span!(id, span, "message" = "enter", |event| {
            drop(extensions);
            drop(span);
            self.on_event(&event, ctx);
        });
    }

    fn on_exit(&self, id: &span::Id, ctx: Context<'_, S>) {
        let span = ctx.span(id).expect("Span not found, this is a bug");
        let extensions = span.extensions_mut();

        with_event_from_span!(id, span, "message" = "exit", |event| {
            drop(extensions);
            drop(span);
            self.on_event(&event, ctx);
        });
    }

    fn on_close(&self, id: span::Id, ctx: Context<'_, S>) {
        let span = ctx.span(&id).expect("Span not found, this is a bug");
        let extensions = span.extensions();
        with_event_from_span!(id, span, "message" = "close", |event| {
            drop(extensions);
            drop(span);
            self.on_event(&event, ctx);
        });
    }

    fn on_event(&self, event: &tracing::Event<'_>, ctx: tracing_subscriber::layer::Context<'_, S>) {
        thread_local! {
            static BUF: RefCell<String> = const { RefCell::new(String::new()) };
        }

        BUF.with(|buf| {
            let borrow = buf.try_borrow_mut();
            let mut a;
            let mut b;
            let mut buf = match borrow {
                Ok(buf) => {
                    a = buf;
                    &mut *a
                }
                _ => {
                    b = String::new();
                    &mut b
                }
            };

            if format_event(
                &ctx,
                tracing_subscriber::fmt::format::Writer::new(&mut buf),
                event,
            )
            .is_ok()
            {
                self.send_message(event, buf);
            } else {
                let err_msg = format!(
                    "Unable to format the following event. Name: {}; Fields: {:?}\n",
                    event.metadata().name(),
                    event.fields()
                );

                eprintln!("[prism-tracing-subscriber] : {}\n", err_msg);
            }

            buf.clear();
        });
    }
}

const TRACE_STR: &str = "TRACE";
const DEBUG_STR: &str = "DEBUG";
const INFO_STR: &str = " INFO";
const WARN_STR: &str = " WARN";
const ERROR_STR: &str = "ERROR";

fn format_event<'a, S>(
    ctx: &Context<'a, S>,
    mut writer: tracing_subscriber::fmt::format::Writer<'_>,
    event: &'a tracing::Event<'a>,
) -> fmt::Result
where
    S: tracing::Subscriber + for<'b> tracing_subscriber::registry::LookupSpan<'b>,
{
    let meta = event.metadata();

    write!(
        writer,
        "{} ",
        match *meta.level() {
            tracing::Level::TRACE => TRACE_STR,
            tracing::Level::DEBUG => DEBUG_STR,
            tracing::Level::INFO => INFO_STR,
            tracing::Level::WARN => WARN_STR,
            tracing::Level::ERROR => ERROR_STR,
        }
    )?;

    let current_thread = std::thread::current();
    if let Some(name) = current_thread.name() {
        'thread_name: {
            use std::sync::atomic::{
                AtomicUsize,
                Ordering::{AcqRel, Acquire, Relaxed},
            };

            // Track the longest thread name length we've seen so far in an atomic,
            // so that it can be updated by any thread.
            static MAX_LEN: AtomicUsize = AtomicUsize::new(0);
            let len = name.len();
            // Snapshot the current max thread name length.
            let mut max_len = MAX_LEN.load(Relaxed);

            while len > max_len {
                // Try to set a new max length, if it is still the value we took a
                // snapshot of.
                match MAX_LEN.compare_exchange(max_len, len, AcqRel, Acquire) {
                    // We successfully set the new max value
                    Ok(_) => break 'thread_name,
                    // Another thread set a new max value since we last observed
                    // it! It's possible that the new length is actually longer than
                    // ours, so we'll loop again and check whether our length is
                    // still the longest. If not, we'll just use the newer value.
                    Err(actual) => max_len = actual,
                }
            }
            // pad thread name using `max_len`
            write!(writer, "{:>width$} ", name, width = max_len)?;
        }
    };

    write!(writer, "{:0>2?} ", current_thread.id())?;

    let mut v = DefaultVisitor::new(writer.by_ref(), true);
    event.record(&mut v);
    v.finish()?;

    for span in ctx
        .event_scope(event)
        .into_iter()
        .flat_map(tracing_subscriber::registry::Scope::from_root)
    {
        let exts = span.extensions();
        if let Some(fields) =
            exts.get::<FormattedFields<tracing_subscriber::fmt::format::Compact>>()
        {
            if !fields.is_empty() {
                write!(writer, " {}", &fields.fields)?;
            }
        }
    }

    Ok(())
}
