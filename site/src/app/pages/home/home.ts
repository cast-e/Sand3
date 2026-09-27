import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterLink } from '@angular/router';
import { ApiService, GitHubRelease } from '../../core/services/api.service';
import { MarkdownPipe } from '../../core/pipes/markdown.pipe';

@Component({
  selector: 'app-home',
  standalone: true,
  imports: [CommonModule, RouterLink, MarkdownPipe],
  templateUrl: './home.html',
  styleUrl: './home.scss'
})
export class HomeComponent implements OnInit {
  private api = inject(ApiService);

  latestRelease = signal<GitHubRelease | null>(null);
  allReleases = signal<GitHubRelease[]>([]);
  readmeContent = signal<string>('');
  instructions = signal<any>(null);
  activeTab = signal<'linux' | 'windows'>('linux');
  loading = signal<boolean>(true);

  ngOnInit() {
    // Detect OS
    if (typeof navigator !== 'undefined' && navigator.userAgent) {
      if (navigator.userAgent.includes('Win')) {
        this.activeTab.set('windows');
      } else {
        this.activeTab.set('linux');
      }
    }

    // Load data
    this.api.getLatestRelease().subscribe({
      next: (rel) => this.latestRelease.set(rel),
      error: (e) => console.error('Failed to load latest release', e)
    });

    this.api.getAllReleases().subscribe({
      next: (rels) => this.allReleases.set(rels),
      error: (e) => console.error('Failed to load all releases', e)
    });

    this.api.getReadme().subscribe({
      next: (data) => this.readmeContent.set(data.content),
      error: (e) => console.error('Failed to load README', e)
    });

    this.api.getInstructions().subscribe({
      next: (data) => {
        this.instructions.set(data);
        this.loading.set(false);
      },
      error: () => this.loading.set(false)
    });
  }

  formatBytes(bytes: number): string {
    if (!bytes || bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }

  getLinuxAsset() {
    const rel = this.latestRelease();
    if (!rel || !rel.assets) return null;
    return rel.assets.find((a) => a.name.includes('linux'));
  }

  getWindowsAsset() {
    const rel = this.latestRelease();
    if (!rel || !rel.assets) return null;
    return rel.assets.find((a) => a.name.includes('win64') || a.name.includes('windows'));
  }
}
