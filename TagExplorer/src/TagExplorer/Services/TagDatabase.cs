using System.IO;
using Microsoft.Data.Sqlite;
using TagExplorer.Models;

namespace TagExplorer.Services;

public sealed class TagDatabase
{
    private readonly string _connectionString;

    public TagDatabase()
    {
        var directory = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "TagExplorer");
        Directory.CreateDirectory(directory);
        _connectionString = new SqliteConnectionStringBuilder
        {
            DataSource = System.IO.Path.Combine(directory, "catalog.db"),
            ForeignKeys = true
        }.ToString();
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            CREATE TABLE IF NOT EXISTS Images(Path TEXT PRIMARY KEY COLLATE NOCASE);
            CREATE TABLE IF NOT EXISTS Tags(Id INTEGER PRIMARY KEY, Name TEXT NOT NULL COLLATE NOCASE UNIQUE, Color TEXT NOT NULL DEFAULT '#5B8DEF');
            CREATE TABLE IF NOT EXISTS PhotoTags(PhotoPath TEXT NOT NULL COLLATE NOCASE, TagId INTEGER NOT NULL REFERENCES Tags(Id) ON DELETE CASCADE, PRIMARY KEY(PhotoPath, TagId));
            CREATE INDEX IF NOT EXISTS IX_PhotoTags_TagId ON PhotoTags(TagId);
            INSERT OR IGNORE INTO Images(Path) SELECT DISTINCT PhotoPath FROM PhotoTags;
            """;
        command.ExecuteNonQuery();
    }

    private SqliteConnection Open() { var connection = new SqliteConnection(_connectionString); connection.Open(); return connection; }

    public void RegisterImages(IEnumerable<string> paths)
    {
        using var connection = Open(); using var transaction = connection.BeginTransaction();
        foreach (var path in paths.Where(File.Exists).Select(System.IO.Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            using var command = connection.CreateCommand(); command.Transaction = transaction;
            command.CommandText = "INSERT OR IGNORE INTO Images(Path) VALUES($path)";
            command.Parameters.AddWithValue("$path", path); command.ExecuteNonQuery();
        }
        transaction.Commit();
    }

    public List<TagInfo> GetTags()
    {
        using var connection = Open(); using var command = connection.CreateCommand();
        command.CommandText = "SELECT Id, Name, Color FROM Tags ORDER BY Name";
        using var reader = command.ExecuteReader(); var tags = new List<TagInfo>();
        while (reader.Read()) tags.Add(new(reader.GetInt64(0), reader.GetString(1), reader.GetString(2)));
        return tags;
    }

    public void EnsureTag(string name, string color = "#5B8DEF")
    {
        name = name.Trim(); if (name.Length == 0) return;
        using var connection = Open(); using var command = connection.CreateCommand();
        command.CommandText = "INSERT INTO Tags(Name, Color) VALUES($name, $color) ON CONFLICT(Name) DO NOTHING";
        command.Parameters.AddWithValue("$name", name); command.Parameters.AddWithValue("$color", color); command.ExecuteNonQuery();
    }

    public void ApplyTags(IEnumerable<string> paths, IEnumerable<string> tags)
    {
        var cleanTags = tags.Select(t => t.Trim()).Where(t => t.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        using var connection = Open(); using var transaction = connection.BeginTransaction();
        foreach (var path in paths.Where(File.Exists).Select(System.IO.Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            using (var image = connection.CreateCommand())
            {
                image.Transaction = transaction; image.CommandText = "INSERT OR IGNORE INTO Images(Path) VALUES($path)";
                image.Parameters.AddWithValue("$path", path); image.ExecuteNonQuery();
            }
            foreach (var tag in cleanTags)
            {
                using var command = connection.CreateCommand(); command.Transaction = transaction;
                command.CommandText = "INSERT INTO Tags(Name) VALUES($name) ON CONFLICT(Name) DO NOTHING; INSERT OR IGNORE INTO PhotoTags(PhotoPath, TagId) SELECT $path, Id FROM Tags WHERE Name=$name COLLATE NOCASE";
                command.Parameters.AddWithValue("$name", tag); command.Parameters.AddWithValue("$path", path); command.ExecuteNonQuery();
            }
        }
        transaction.Commit();
    }

    public List<PhotoItem> Search(string? query = null, string? tag = null)
    {
        using var connection = Open(); using var command = connection.CreateCommand();
        command.CommandText = """
            SELECT i.Path, COALESCE((SELECT group_concat(t.Name, ', ') FROM Tags t JOIN PhotoTags pt ON pt.TagId=t.Id WHERE pt.PhotoPath=i.Path), '')
            FROM Images i
            WHERE ($tag IS NULL OR EXISTS(SELECT 1 FROM PhotoTags p JOIN Tags t ON t.Id=p.TagId WHERE p.PhotoPath=i.Path AND t.Name=$tag COLLATE NOCASE))
              AND ($query IS NULL OR i.Path LIKE $like OR EXISTS(SELECT 1 FROM PhotoTags p JOIN Tags t ON t.Id=p.TagId WHERE p.PhotoPath=i.Path AND t.Name LIKE $like))
            ORDER BY i.Path;
            """;
        command.Parameters.AddWithValue("$tag", (object?)tag ?? DBNull.Value);
        command.Parameters.AddWithValue("$query", string.IsNullOrWhiteSpace(query) ? DBNull.Value : query.Trim());
        command.Parameters.AddWithValue("$like", "%" + (query?.Trim() ?? "") + "%");
        using var reader = command.ExecuteReader(); var results = new List<PhotoItem>();
        while (reader.Read()) { var path = reader.GetString(0); if (File.Exists(path)) results.Add(new() { Path = path, Tags = reader.GetString(1) }); }
        return results;
    }
}