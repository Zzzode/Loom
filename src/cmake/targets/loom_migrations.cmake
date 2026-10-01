# ─── loom_migrations: Schema Migrations ─────────────────────────────────────────
add_library(loom_migrations)
target_sources(loom_migrations
    PRIVATE
        migrations/concrete_migrations.cpp
    PUBLIC FILE_SET CXX_MODULES FILES
        migrations/concrete_migrations.cppm
        migrations/migration_runner.cppm
        migrations/schema_versions.cppm
        migrations/lockfile.cppm
)
target_link_libraries(loom_migrations PUBLIC loom_utils)
